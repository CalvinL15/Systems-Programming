Name: Calvin (???)
B06902100

First subtask: the code compiles successfully.

Second subtask: the code looks for the specified item in item_list and return the value of the specified item after reading the input.

Third subtask: the code rewrites the data (price or amount) of the specified item in accordance with the instruction of user. If the price is set to <= 0, 
the operation failed since it does not make any sense. Operation failed also occurs when: sell or buy <= 0 (also does not make sense), trying to buy more
than the amount available, or when the client try to do other command other than price, sell or buy. 

Fourth subtask: by utilizing select function, the code is able read the data of inputted item for each client accordingly

Fifth subtask: by utilizing select function, the code can respond to other connection(user) without losing track of the connection which has not send
its action command. I declare a new variable "rwait" that is used to differentiate which data the server want to receive from the client. 

Sixth subtask: the code utilize an array of integers as flags to block the request to modify the item which another client has requested on single server. 

Seventh subtask: the code utilize fcntl() function to add a lock to the specified item when a client is reading (F_RDLCK) or is writing (F_WRLCK), such that
when there is/are another clients that try to read or write the same item as the specified one, the file is protected (output: This item is locked). After the 
client finishes the request, the lock is lifted (F_UNLCK). 
