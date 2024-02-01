`make start` to start and log into the container

### some rough ideas

zcs_init: (check app or service node)
- if app
  1. create an app to service node multicast channel
  2. create a local table 
  3. send DISCOVERY - once
  4. listen NOTIFICATION, HEARTBEAT periodically (in another thread)
      - update the local table
- if service (assume a service node doesn't maintain a local table)
  1. create a service to app multicast channel

zcs_start: (only for the service node)
1. send NOTIFICATION - once
2. listen DISCOVERY periodically (in another thread)
   - if DISCOVERY received, send NOTIFICATION 
3. send HEARTBEAT periodically (in another thread)

zcs_listen_ad
  update the received service node status in the local table with the corresponding callback pointer.
  look up the table when a message is received. The receiving message will have the sender's name. use that and find the function to run. 


### message format
- DISCOVERY: "type=DISCOVERY"
- HEARTBEAT: "type=HEARTBEAT&name=[serviceName]"
- NOTIFICATION: "type=NOTIFICATION&name=[serviceName]&[attr_name]=[attr_value]"