# Description of the Implemented Solution

## Solution Overview

### Inter Process communication

Since in the context of the assignment is local (the server and the client runs on the same computer) I choose the implementation of named pipes between the client and server process. There is one main pipe that provides communication TO the the server. Every client writes to this one pipe when it wants to send data to the server. The server constantly reads this pipe. Every message starts with "\<client name>:" then the message. This way the server knows which client it needs to reply.

The answer FROM the server to the client is done by individual named pipes to every client on a pipe that contains their client id. Every client reads the response from the pipe that contains their own id.

I choose this type of communication because we had to use it in the self-learning assignment and I think it is more straight forward than using sockets.

![Alt text](<documentation/src/Screenshot 2024-01-17 at 18.11.04.png>)

### The Game

I see the game as a step by step process (i.e. Game starts, wait for players, rounds, moves in every round). This is why I choose to use threads. When a game starts a new thread will open and this flow of processes start there separately. The data of the game is stored in global variables, so the game thread reads that constantly and the main thread writes to it when there is a change (e.g. players joins, move happens). In order to avoid problems with reading/writing I use mutex locks. I tried to find a way for the most efficient wait implementation because the game has to know when a game is full, so it has to check the status time-to-time. My first implementation was to check it every second but it was not efficient. I used thread signals instead. The game only checks if the number of players when a signal comes that a player is joined to a game.

![The game](<documentation/src/Screenshot 2024-01-17 at 18.15.01.png>)

## Detailed Description

All the functions of Client and Server are in the gameLogic.c file. I put comments everywhere explaining what parts of the code does.

I used the ncurses library in order to make the console output and game usage as best looking as possible. I used the same library to read user inputs, this way no hitting enter is required. When a key is pressed the corresponding action will happen if it is was a valid option.

The data transfer as I mentioned earlier happens through pipes. The way of data is sent is a kind of a implementation of a control code system. I found this the most efficient. When a client send information to the server it starts with its name and a control block (e.g. GI for game info). If more information is needed (e.g the game id) it is also put to this control string. The information sent back from the server contains data chained together into a string the same way (e.g GI;2;p1,p2;1,3;0,0;rn means in this game there are 2 players named p1 and p2. It is the first round out of 3. They both have 0 points. p1 made a move -r- and p2 didn't make move yet.). The client gets all necessary information for it and outputs it in a user friendly format.

When a client exits the program it cannot join back. There can't be 2 clients with the same name. This is handled on the server side when a client tries to join.

In the case of the server terminates the client only exits when tries to do some action to the server the next time. This is because the client is not reading/writing the pipe constantly, only when some action happens.

I managed to implement all game functionalities that was mentioned in the description.

I tried to reproduce the exact output that was shown in the description. I think I managed to do it everywhere. A few pictures about the program:

![Alt text](<documentation/src/Screenshot 2024-01-17 at 17.54.29.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 17.55.17.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 17.58.29.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 17.58.56.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 17.59.14.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 18.00.47.png>)\
![Alt text](<documentation/src/Screenshot 2024-01-17 at 18.01.17.png>)

## Quality Assurance

### Testing

With unit tests I tried to simulate all functionalities of the program, for example multiple users and multiple mages. Basically I tested all server side functions.

Unfortunately because of the library I used i couldn't figure out a way to test the client side prints.

I also tested during the process and I tried every functionality multiple times (i.e. I tried to play games many times etc). Because of this I didn't feel the need of testing the prints with additional unit tests.

There was an interesting thing that I couldn't figure out. Even though I tried to use locks, signals the right way I had to put a few microsecond waits between function calls, because without them the tests got stuck like they were in deadlock. It still happens sometimes when I run the tests but rarely.

I know the coverage is low, but it is because I couldn't test the client side with unit tests. Every real game functionality is tested on the server side.

![Running tests](<documentation/src/Screenshot 2024-01-17 at 17.41.52.png>)

## Notes

Because of the ncurses library I have my own makefile that builds my program. It uses this gcc command:
> gcc -g -Wall -o Main Main.c -lncurses

The tests also have to be run with an addition:
> MORE_ARGS="-lncurses" make -f public/makefile test
