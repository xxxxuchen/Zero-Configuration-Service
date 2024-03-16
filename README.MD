## HOW TO RUN THE CODE

**Note: For PART-2 Specific documentation skip to Extended Config Section below.**

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

## Part 2: Extended Zero Config

### HOW TO RUN THE CODE IN A SINGLE MACHINE
- `cd src`. you MUST first to go to the **src** folder.
- type `make` to compile the code.
- Below is just an example of how to connect LAN-A and LAN-B by activating the relay.
  - `./service 224.1.10.1 224.1.10.2` Start the service node in LAN-A
  - `./app 224.1.10.1 224.1.10.2` Start the app node in LAN-A
  - `./service-2 224.1.20.1 224.1.20.2` Start another service node in LAN-B
  - `./app-2 224.1.20.1 224.1.20.2` Start another app node in LAN-B
  - `./relay 224.1.10.1 224.1.10.2 224.1.20.1 224.1.20.2` Start the relay 
- You can also see the relay is working by simply starting two nodes:
  - `./service 224.1.10.1 224.1.10.2` Start the service node in LAN-A
  - `./app 224.1.20.1 224.1.20.2` Start the app node in LAN-B
  - `./relay 224.1.10.1 224.1.10.2 224.1.20.1 224.1.20.2` Start the relay 

### Changes:

- The first important change from part 1 is that the apps and services of a LAN must receive the adresses of the apps and services of that LAN.
  - For example: Running `./app 224.1.10.1 224.1.10.2` and `./service 224.1.10.1 224.1.10.2` will start App-1 and Service-1 on LAN-A such that the app and service will communicate **exactly** as they did in **Part 1** of the assignment.
  - Note that the order of arguments does **not** change: **the first argument is for apps and the second is for services**
  - In this example, LAN-A will involve apps with address `224.1.10.1` and services with address `224.1.1.10.2 ` and both app-1 and service-1 will know this from the passed arguments.
- For simplicity, we use the following adresses for our experiments:
  - `224.1.10.1`: LAN-A Apps
  - `224.1.10.2`: LAN-A Services
  - `224.1.20.1`: LAN-B Apps
  - `224.1.20.2`: LAN-B Services
### Relay Setup

- The relay is a process (relay.c) and is run in a separate terminal
- The relay can be started before or after the apps and services are started since it does itself sends Discovery messages to force the Services on either LAN to respond.
- The relay is designed to work with **two LANs** and any number of apps and services but our experiments will uses 2 apps and 2 services spread over the 2 LANs.
- The relay runs **4 separate threads** to handle message forwarding between the two LANs:
  - **listenServicesThread1**: Listens to messages from LAN-A Services --> Forwards to LAN-B Apps
  - **listenServicesThread2**: Listens to messages from LAN-B Services --> Forwards to LAN-A Apps
  - **listenAppsThread1**: Listens to messages from LAN-A Apps --> Forwards to LAN-B Services
  - **listenAppsThread2**: Listens to messages from LAN-B Apps --> Forwards to LAN-A Services
- We add a special tagging string `forward=true` to the messages during forwarding because the relay will do a multicast send when it forwards anything. This means that the relay will also receive any message which it forwards back in one of the listening threads.
- Checking for this tag when listening for a message prevents the relay from being stuck in a forwarding loop since we can discard the message we just forwarded.

### Limitations:

**Having more than 2 LANs:**

The are two general main limitations involved here:

1. The relay expects to receive the addresses at runtime since we pass them as arguments. We would thus have to pass an array of adresses instead of the specifc 4 arguments used in our case.
2. Assuming you pass an array of adresses, and have everything running, you have another issue of wether you want the relay to be adaptive when a new LAN is started or wether you reset the entire relay to re-pass the new adresses at runtime. Resetting the relay would obviouslt cause interruptions to ongoing communications and is undesirable.

**Possible Solution:**

Assuming we designed our relay to be receptive to a list of LAN app/service adresses, we would have issues with the forwarding since we currently have 4 threads only running with each having a specific task of listening and forwarding a message.

It would thus not be possible to know how many threads to initialize in advance; instead, we would need an approach that starts a new thread on command when an app or service is started on any LAN network. This could be done through an Observer design pattern where the relay has some functionality that connects it to any new LAN that wants to join and then each we would have a callback such that any app/service would cause a new thread to start.

This adaptive approach would thus address both limiation mentioned above since the relay doesn't even need to receive the adresses as arguments (they are injected in once a network wants to establish an involve itself in an inter-network).

The threads also do not need to be started at relay runtime but only when an app/service is booted on the inter-network.

---

## BELOW IS NOT FOR TESTING (Part-1 Relevant)

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
