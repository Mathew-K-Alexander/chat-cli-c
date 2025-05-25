## Terminal Based Chat App

Currenly works in Linux-based systems and WSL.

#### Please do the following install:
- $ sudo apt-get install libncurses-dev
</br>

#### To create server and client executables run the following commands:
- $ gcc client.c socketutil.c -o client -lncurses -lpthread

- $ gcc server.c  socketutil.c -o server -lpthread
</br>
  
#### To run the executable run:
  1. In a new window, cd to directory and run: $ ./server `<custom-password>`
  > Note: Replace <custom-password> with your desired password (don’t include the angle brackets).
  2. In another window, cd to directory and run: $ ./client
</br>

#### One can create multiple clients that can connect to the server
</br>

![WhatsApp Image 2025-05-25 at 12 08 06 PM](https://github.com/user-attachments/assets/7e1622bd-6b87-4c0d-b04d-18c082ae9ffe)


