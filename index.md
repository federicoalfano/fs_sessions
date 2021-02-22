# AOSV 2018/2019 Final Project

## File Access with Sessions

For the final exam project, the student is required to implement a subsystem which allows to access files in a specific directory of the VFS using *sessions*.

The implementation can be made by recompiling the kernel, or by realizing a LKM. The decision is given to the student.

The path to the folder in which files should be accessed must be parameterizable. That is, the Linux subsystem should provide an interface which can be used to specify the path. This path can change at runtime (i.e., in case of a module, the path can be changed also without unmounting/remounting the module).

This subsystem shall work in the following way. Let us suppose that the initial path is `/mnt/dir-A/`. All files in that folder, and in *any* subfolder of that path can be opened using sessions. The way according to which the user tells the subsystem that a file shall be opened using sessions is left as a design choice to the student. 

Once a file is opened using session, userspace applications are allowed to run traditional VFS-related syscalls on them. Anyhow, a new *incarnation* of its content is created, which is initialized with the currently-available content of the original file in `/mnt/dir-A/`. Updates to the file opened using sessions are not immediately visibile in the original file in `/mnt/dir-A/`. Rather, the updates in one incarnation are flushed to the original file only once a session is closed by relying on the `close()` syscall.

When a file is opened using sessions and an incarnation of its content is created, the content needs to be initialized atomically with respect to the closure of another incarnation of the same file (which flushes the content of the session file to the original file). The same is true for concurrent closures of different incarnations of a same file opened using sessions.

This is the so-called *session-based file access semantic*, as opposed to the classical UNIX semantic where any update on a file performed in an I/O session is made visible to other I/O sessions on the same file.

When closing a file opened using session, if the original file has been removed, the closing process must receive `SIGPIPE`. Opening a file which does not exist must make the `open()` syscall fail, obviously.

The subsystem must be designed to support working with a large number of files and a large number of sessions. Moreover, the files can be of any size. Therefore, it is not possible to have only a RAM-backed subsystem. It is left as a design strategy how to use disk-storage to back the various file sessions.

Files opened using sessions must support at least the following set of file operations:

- `open`
- `release`
- `read`
- `write`
- `llseek`

Concurrent I/O sessions on files must be allowed, as well as concurrent operations on a same I/O session, as usual for files in Linux. Atomicity of the concurrent operations must still be guaranteed.

If the path for the files accessed using sessions is changed, open sessions must still be managed according to the session-based semantic. This means that any `open()` on the old path will be served using the classical UNIX semantic after the path change, but files which were already opened will still use the session-based semantic.

At any time, userspace applications can query the subsystem to know:

* The total number of opened sessions
* The number of opened sessions for each file and a list of process names and pids which are currently using sessions (in no particular order).

This information can be provided through `procFS` or `sysFS`, at the discretion of the student.
