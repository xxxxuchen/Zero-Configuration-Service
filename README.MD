`make start` to start and log into the container

### some rough ideas

- zcs_init: (treat as app node by default)
  1. create a local table and other global variables
  2. send DISCOVERY - once 
  3. listen NOTIFICATION, HEARTBEAT periodically (in another thread)
     - update the local table 

- zcs_start: (app node  -> service node)
  1. send NOTIFICATION - once
  2. listen DISCOVERY periodically (in another thread)
     - if DISCOVERY received, send NOTIFICATION 
  3. send HEARTBEAT periodically (in another thread)

- zcs_listen_ad
  update the received service node status in the local table with the corresponding callback pointer.
  look up the table when a message is received. The receiving message will have the sender's name. use that and find the function to run. 