# ftpserver
:computer: A small FTP-like server (really basic though), written in C by @vincent38 and @VonShtarb (remotely)

## How to build ?

Just do make on a Linux-based OS. (Windows unsupported)

## How to use it ?

Drop your serverFTP and the .mnet files on the server computer, open the port 2121, and let it run. 
Be sure to drop it close to the files that needs to be available.

Then, open the clientFTP file on the client computer, and you will be able to use the basic commands directly.

The .mnet file is the accounts database, the server needs it to run. It contains the usernames and hashed passwords.
The storage system won't be changed as the project is provided "As it is".

A test account is provided on the .mnet file. Please login with this account, create a new one through addusr (see below), and
delete the test account for safety reasons !

Test account credentials : username - *megumi*, password - *cureFlora*

## Basic commands

* get FILENAME -> Download file FILENAME from server
* recover -> Downloads the missing part of the last file downloaded if an error occured during the process (disconnected, ...)
* ls -> Lists the contents of the current directory
* pwd -> Prints the working directory
* cd -> Changes the current directory
* auth USERNAME -> Login to an admin account (prompts for the user password)
* bye -> Logout and disconnect from the server

## Admin commands

* mkdir FOLDER -> Create a new folder
* rm FILENAME -> Delete file FILENAME
* rm -r FOLDER -> Delete folder FOLDER and all it's contents
* addusr NEW_USER -> Prompts for new password, and creates a new user.
