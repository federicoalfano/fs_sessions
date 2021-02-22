/**
 * @file libsession/libsession.h
 * @author Federico Alfano
 * @date 2 May, 2020
 * @brief The header of the userspace library that allows the user to start a session based file access
 *
 */
#ifndef SESSION
#define SESSION


/**
 * @brief session_init express the wish to initialize a session
 * @return the id representing the session if everything goes well, otherwise returns an error code
 */
extern int session_init(void);

/**
 * @brief opens a session on a specific file descriptor
 * @param session_id the id returned by the init
 * @param fd the file descriptor on which the session must be open
 * @return 0 if everything goes well, otherwise returns an error code
 */
extern int session_open(int , int );

 /**
 * @brief closes a session on a specific file descriptor
 * @param session_id the id returned by the init
 * @param fd the file descriptor on which the session must be close
 * @return 0 if everything goes well, otherwise returns an error code 
 */
extern int session_close(int , int );

/**
 * @brief session_exit expresses the whish of terminating the session 
 * @param session_id the id returned by the init
 * @return 0 if everything goes well, otherwise returns an error code
 */
extern int session_exit(int );

#endif



