Hey there! Welcome to the Terminal Lift Simulator.

This is a multi-process elevator simulation built entirely in C. It uses `ncurses` to draw a clean, real-time UI right in your terminal. 
Under the hood it uses threads, real-time Inter-Process Communication (IPC), and dynamic/static library linking.

The project is split into two parts:
* **The Brain (`main`):** Handles all the math, physics, state tracking, and logic.
* **The Face (`child`):** Takes that state data and renders the graphics in your terminal. 


## What You Need to Install

Since this is built in C and uses terminal graphics, you'll need a C compiler and the `ncurses` library. 

If you're on a Debian/Ubuntu-based Linux setup, just open your terminal and run this:

```bash
sudo apt update
sudo apt install build-essential libncurses5-dev libncursesw5-dev
```

Since this project was built with linux in mind it is advised to run it in a linux environment, even a WSL works !!

## How to make ?

You could either choose the static or dynamic version
 - run make static - all of it baked into the binaries
 - run make shared - depends on the shared libraries created by make

## Controls

the number of floors you need should be specified as an argument while running main
Controls are pretty intuitive, arrow keys for moving the grid of floors and enter for selecting that floor, mouse is supported too
q for closing the window
If you wish to stop, you could just kill the main binary and the children would be automatically cleaned before exit

