## HOW TO RUN THE CODE


### 1. Using Our Custom Docker Image
You can use our custom Docker image for testing. But there are some steps you need to follow before you can run the code.
- we used the docker-compose file to build the image and run the container. 
- make sure you are inside the folder where the docker-compose file is located.
- type `make start` to run and log in to the container.
- now make sure you are inside the /home/zcs folder where all the c files are located.
- type `make` to compile the code.
- four executables will be created, which you can use for testing: `app`, `app-2`, `service`, and `service-2`.


### 2. Without Using Our Custom Docker Image
- `cd src`. you MUST first to go to the **src** folder.
- type `make` to compile the code.

### Some Notes
> All the threads are running in a predefined time interval. So you may need to wait for a few seconds to see the output. For example, you should see the DOWN message logged in the console within 5 seconds after you terminate the service node.

> You can define your own ads sending frequency and duration in the service client file. The default sending interval is 10 seconds, so you should also expect to wait for some time to see the ad messages logged in the console.

**BELOW IS NOT FOR TESTING**

---
---

### some rough ideas

zcs_init: (check app or service node)
- if app
  1. create an app to service node multicast sending channel
  2. create a local table 
  3. send DISCOVERY - once
  4. create a app receiving channel
  5. listen NOTIFICATION, HEARTBEAT, and advertisement periodically (in another thread)
      - update the local table or call the callback function
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
- DISCOVERY: "message_type=DISCOVERY"
- HEARTBEAT: "message_type=HEARTBEAT&name=[serviceName]"
- NOTIFICATION: "message_type=NOTIFICATION&name=[serviceName]&[attr_name1]=[attr_value1]&[attr_name2]=[attr_value2]..."
- advertisement: "message_type=advertisement&name=[serviceName]&[ad_name]=[ad_value]"
