# rslogin
This was a tool I wrote used to automatically login to remote hosts. It supported SSH key authorization and automatic password
changes. Originally this was an extremely large expect script which caused delays in login. It was converted into C code that
generates an expect script to login to the host specified and then executes it. As such it is required that expect be installed
on your system before use. If you still use local system authentication this can be useful so you can keep passwords different
and rotating on a per host or per host group basis. Password list can also be gpg encrypted. I have yet to write any real documentation
as I no longer use this tool and stopped updating it long ago. However the post-compile usage help and configuration templates "should"
be relatively self-explanatory. Use at your own risk.
