`make start` to start and log into the container

---

`cd /src` 
- `make app`
- `make service`

### some rough ideas

zcs_init: (check app or service node)
- if app
  1. create an app to service node multicast sending channel
  2. create a local table 
  3. send DISCOVERY - once
  4. create a app receiving channel
  5. listen NOTIFICATION, HEARTBEAT periodically (in another thread)
      - update the local table
- if service (assume a service node doesn't maintain a local table)
  

zcs_start: (only for the service node)
1. create a service sending channel
2. send NOTIFICATION - once
3. create a service receiving channel
4. listen DISCOVERY periodically (in another thread)
   - if DISCOVERY received, send NOTIFICATION 
5. send HEARTBEAT periodically (in another thread)

zcs_listen_ad:
  Update the received service node status in the local table with the corresponding callback pointer.
  Look up the table when a message is received. The receiving message will have the sender's name. use that and find the function to run. 


### message format
- DISCOVERY: "type=DISCOVERY"
- HEARTBEAT: "type=HEARTBEAT&name=[serviceName]"
- NOTIFICATION: "type=NOTIFICATION&name=[serviceName]&[attr_name1]=[attr_value1]&[attr_name2]=[attr_value2]..."
- advertisement: "type=advertisement&name=[serviceName]&[ad_name]=[ad_value]"