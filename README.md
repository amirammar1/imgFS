<img src="done/resources/Pictures/EPFL.png" title="EPFL logo" width="100"  align="right" sizes="auto" alt="2">

# ImageFS CS202-2024, by Amir Ammar and Yassine Kouch

## Description
ImageFS is the programming project for the second semester 2024 of the second-year Bachelor's degree in Communication Science 
at EPFL. The aim of this project is to improve our use of C language, in particular advanced concepts and network.
This course is given by **Mr. Bugnion**, **Mr. Chappelier** and **Mrs. Argyraki**, you can find all the description of our course and the project instructions on the site
dedicated to his course [CS-202](https://projprogsys-epfl.github.io/
).

## Interface
There is two ways to use our tools for ImageFS, There is cmd option with [imgfscmd](imgfscmd.c) or you can use the tools with our server coded in [image server](imgfs_server.c).
To convert the C code to machine code just type ```make```.

And then, to use this tools you can either type: 


<font color="red">For CMD : </font>
```bash
./imgfscmd help 
```
and you will get the description how to use it.

<font color="red">For server : </font>
```bash
./imgfs_server <ImgFS_PATH_YOU_WANT_TO_EDIT> <OPTIONAL_PORT_NUMBER>
```

## Bonus part
In the [http_get_var](http_prot.c) we added additional verification for the URL to check if there is <font color="orange">"?"</font> or <font color="orange">"&"</font> before the name.

We also added a function [```read_message```](http_net.c) to avoid code duplication in [```handle_connection```](http_net.c).

In the [```http_reply```](http_net.c), we optimised our memory allocation by sending the header and the body separately, because as studied, two sends is equivalent to one send since ```send``` ignore <font color="orange">"\0"</font>.

## Additional remarks
In the [```tcp_server_init```](socket_layer.c), due to a blocking problem of address already assigned when using the same port we added a bloc of code to enable using the same port and avoid wasting time waiting for availability of port(without the added bloc, random end-to-end tests fails in our machines due to the environment we use).

## Testing
During this project, we used the end-to-end and unit tests given by CS202-team.\
To run them use simply the command:
```bash
make check
```
We also coded [tcp-test-server.c](tcp-test-server.c) and [tcp-test-client.c](tcp-test-client.c) to check our basic implementation of tcp command by sending and receiving small text messages and displaying them.\
To run them, simply open two terminals in parallel and type:

<font color="red">For server : </font>
```bash
./tcp-test-server <PORT_NUMBER>
```
<font color="red">For client : </font>
```bash
./tcp-test-client <PATH_TEXT_FILE_TO_SEND>
```

