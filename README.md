# Client-Server Application for Customer Loyalty Program (Sunspots)

This project implements a socket-based client-server application designed to manage a customer loyalty program, referred to as "Sunspots." The system allows multiple clients to connect simultaneously to the server and exchange information over TCP/IP sockets.

Key Features:
Client-Server Communication: Clients send requests to the server to retrieve and update customer loyalty points.
Concurrent Client Handling: The server is capable of managing multiple clients concurrently using a forking or multi-threading approach, ensuring that each client's request is processed efficiently.
Data Management: The server handles reading from and writing to a data store, ensuring that customer records (loyalty points) are updated in real-time.
Socket Programming: Communication between clients and the server is handled via TCP/IP sockets, ensuring reliable data exchange.

Overall, this project demonstrates basic concepts of network programming, concurrency, and real-time data management in a UNIX/Linux environment.


-- 

Done in accordance with these guidelines: https://www.cs.utoronto.ca/~trebla/CSCB09-2024-Summer/a4/cscb09-2024-5-a4.html
