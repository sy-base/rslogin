//#RSLOGIN: remote slogin to automatically ssh into remote systems
//#RSLOGIN: Written by Jason Curtis (C)2001-2007

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sys/utsname.h>
#include <sys/types.h>
#include <pwd.h>
#include <netdb.h>
using namespace std;


// Constants
#define PNAME "rslogin"
#define VERSION "0.32.5"
#define DEVSTATE "DEV"
#define MAX_HOSTGROUPS 50
#define MAX_HOSTS 255
//
// Global Variables and their defaults
int debug=0;
int msglvl=0;
char *CONFIG_FILENAME="/etc/rslogin.cfg";
string USERHOME;
string RSHOME;
string RSEXPECT="rslogin.x"; 
string RSPASS="rslogin.pass";
string RSENCPASS=RSPASS+".gpg";
string SSHBIN="/usr/bin/slogin";
string EXPECTBIN="/usr/bin/expect";
string GPGBIN="/usr/bin/gpg";
bool USE_GPG;
bool VERBOSE_LOGIN=bool(1);
bool USE_AUTOPASS;
bool USE_PASSREVERT;
bool USE_AUTOROOT;
string NAGROUP="NETAPP";
string USERLOGIN;
string USERPASS[2];
string NAPASS;
string LOGINHOST;
string LOGINHOSTGROUP;

//Create a struct "DISTRO" for parsing the password file and root distro file.
struct DISTRO {
	string NAME;
	string VALUE;
	string UP1;
	string UP2;
} HOSTGROUP[MAX_HOSTGROUPS], HOST[MAX_HOSTS], DFHOSTGROUP[MAX_HOSTGROUPS], DFHOST[MAX_HOSTS];
int HOSTCOUNT=0;
int HOSTGROUPCOUNT=0;

string gethome() {
	struct utsname si;
	string sysname = "Linux";
	string username;
	string userhome;
	//set struct pw to the username based on the proccess user id
	passwd *pw = getpwuid(getuid());
	//call uname function to populare the struct si with operating system information
	int sierr=uname(&si);
	
	// check if uname function returned an error, if it did the default Linux will be used
	if (sierr < 0) {
		// debug msglvl > 3
		if (msglvl > 3) {
			cout << "sierr = " << sierr << endl;
		}
		cout << "Unable to get sysname, Default: Linux" << endl;
	} else {
		sysname = si.sysname;
		//debug msglvl > 1
		if (msglvl > 1) cout << "System Name: " << sysname << endl;
	}
	
	/* Check the sysname string to determine the type of system we are running on, then set the home direcctory base accordingly.
		Currently we only support Darwin user home directories and all others will use /home. This can obviously be easily
		changed in the future.
	*/
	if (sysname.compare("Darwin") == 0) {		// Support for MacOSX home directory structure
		userhome = "/Users/";
	} else if (sysname.compare("SunOS") == 0) { // Support for Solaris (SunOS) home directory structure
		userhome = "/export/home/";
	} else {									// Default to Linux home directory structure

		userhome = "/home/";
	}
	// retrieve the username from the pw struct we previously created and append it to the string "userhome" with a trailing slash
	username = pw->pw_name;
	userhome.append(username);
	userhome.append("/");

	// return the completed user home directory
	return userhome;
}


int read_config (char *config_filename) {
	string cfgline;
	string cfgname;
	string cfgval;
	size_t delpos;
	
	//debug msglvl > 0
	if (msglvl > 0) cout << "Reading User Configuration file: " << config_filename << endl;

	// create conffile file stream
	ifstream conffile;
	//first try to open user configuration file (i.e. ~/.rslogin/rslogin.cfg)
	conffile.open( config_filename );
	if (conffile.is_open() == 0) {
		//debug msglvl > 0
		if (msglvl > 0) cout << "No User Configuration file, Trying Default: /etc/rslogin.cfg" << endl;
		//opening user configuration file failed. Try to open global system configuration (i.e. /etc/rslogin.cfg)
		conffile.open ( CONFIG_FILENAME );
		if (conffile.is_open() == 0) {
			//Couldn't open global configuration either, return with -1
			cout << "Unable to open configuration file!" << endl;
			return(-1);
		}
	}
	// read config file and parse known attributes, discard lines beginning with # and unknown attribute names.
	while (conffile.good()) {
		getline(conffile, cfgline); 
		delpos = cfgline.find("=");
		if (delpos != string::npos) {
			cfgname = cfgline.substr(0,delpos);
			cfgval = cfgline.substr(delpos+1);
			if (cfgname.compare("RSEXPECT") == 0) { 
				RSEXPECT = cfgval;
			} else if (cfgname.compare("RSENCPASS") == 0) {
				RSENCPASS = cfgval;
			} else if (cfgname.compare("RSPASS") == 0) {
				RSPASS = cfgval;
			} else if (cfgname.compare("SSHBIN") == 0) {
				SSHBIN = cfgval;
			} else if (cfgname.compare("EXPECTBIN") == 0) {
				EXPECTBIN = cfgval;
			} else if (cfgname.compare("GPGBIN") == 0) {
				GPGBIN = cfgval;
			} else if (cfgname.compare("USEGPG") == 0) {
				USE_GPG = bool(cfgval.compare("0"));
			} else if (cfgname.compare("USE_GPG") == 0) {
				USE_GPG = bool(cfgval.compare("0"));
			} else if (cfgname.compare("VERBOSE_LOGIN") == 0) {
				VERBOSE_LOGIN = bool(cfgval.compare("0"));
			} else if (cfgname.compare("USE_AUTOROOT") == 0) {
				USE_AUTOROOT = bool(cfgval.compare("0"));
			} else if (cfgname.compare("USE_AUTOPASS") == 0) {
				USE_AUTOPASS = bool(cfgval.compare("0"));
			} else if (cfgname.compare("USE_PASSREVERT") == 0) {
				USE_PASSREVERT = bool(cfgval.compare("0"));
			} else if (cfgname.compare("NAGROUP") == 0) {
				NAGROUP = cfgval;
			} else if (cfgname.compare("RSHOME") == 0) {
				RSHOME = cfgval;
			} else if (cfgname.compare(0,1,"#") == 0) {
				//debug msglvl > 3
				if (msglvl > 3) cout << "Skipping Commented Configuration Attribute" << endl;
			} else {
				//debug msglvl > 1
				if (msglvl > 1) cout << cfgname << " is an unknown attribute, skipping" << endl;
			}
		}
	}
	conffile.close();
	// if "RSHOME" was not defined in the config file. Define it based on gethome function and append .rslogin
	if (RSHOME.empty()) {
		RSHOME.append(USERHOME);
		RSHOME.append(".rslogin");
	}

	//debug msgslvl > 0
	if (msglvl > 0) cout << "Config Filename: " << config_filename << endl;
	//debug msglvl > 1
	if (msglvl > 1) {
		cout << "\tRSHOME         = " << RSHOME << endl;
		cout << "\tRSEXPECT       = " << RSEXPECT << endl;
		cout << "\tRSPASS         = " << RSPASS << endl;
		cout << "\tRSENCPASS      = " << RSENCPASS << endl;
		cout << "\tSSHBIN         = " << SSHBIN << endl;
		cout << "\tGPGBIN         = " << GPGBIN << endl;
		cout << "\tEXPECTBIN      = " << EXPECTBIN << endl;
		cout << "\tVERBOSE_LOGIN  = " << VERBOSE_LOGIN << endl;
		cout << "\tUSE_AUTOROOT   = " << USE_AUTOROOT << endl;
		cout << "\tUSE_GPG        = " << USE_GPG << endl;
		cout << "\tUSE_AUTOPASS   = " << USE_AUTOPASS << endl;
		cout << "\tUSE_PASSREVERT = " << USE_PASSREVERT << endl;
		cout << "\tNAGROUP        = " << NAGROUP << endl;
	}

    return 0;
}

int read_pass(string password_file) {

	string passline;
	string passtype;
	string passname;
	string passval[4];
	size_t delpos[4];
	
	//Attempt to open password file
	ifstream passfile;
	passfile.open ( password_file.data() );
	//Check if password file is open, if not return
	if (passfile.is_open() == 0) {
		cout << "Unable to open Password file for reading" << endl;
		return(-1);
	}

	// read pass file looking for userlogin,userpass,host(s),hostgroup(s)
	while (passfile.good()) {
		passtype="";
		passname="";
		passval[0]="";
		passval[1]="";
		passval[2]="";
		
		getline(passfile, passline); 
		if (passline.compare(0,1,"#") != 0) {
			delpos[0] = passline.find(":");
			delpos[1] = passline.find(":", delpos[0]+1);
			delpos[2] = passline.find(":",delpos[1]+1);
			delpos[3] = passline.find(":",delpos[2]+1);
			//debug msglvl > 5
			if (msglvl > 5) {
				cout << "Delimeter 1: " << delpos[0] << " Delimeter 2: " << delpos[1] << endl;
			}
		
			if (delpos[0] != string::npos) {
				passtype = passline.substr(0,delpos[0]);
				if (delpos[1] != string::npos) {
					passname = passline.substr(delpos[0]+1,delpos[1]-delpos[0]-1);
					if (delpos[2] != string::npos) {
						passval[0] = passline.substr(delpos[1]+1,delpos[2]-delpos[1]-1);
						if (delpos[3] != string::npos) {
							passval[1] = passline.substr(delpos[2]+1,delpos[3]-delpos[2]-1);
							passval[2] = passline.substr(delpos[3]+1);
						} else {
							passval[1] = passline.substr(delpos[2]+1);
						}
					} else {
						passval[0] = passline.substr(delpos[1]+1);
					}
				} else {
					passname = passline.substr(delpos[0]+1);
				}
								
				if (passtype.compare("USERLOGIN") == 0) {
					USERLOGIN = passname;
				} else if (passtype.compare("USERPASS") == 0) {
					USERPASS[0] = passname;
				} else if (passtype.compare("USERPASS2") == 0) {
					USERPASS[1] = passname;
				} else if (passtype.compare("HOST") == 0) {
					HOST[HOSTCOUNT].NAME = passname;
					HOST[HOSTCOUNT].VALUE = passval[0];
					HOST[HOSTCOUNT].UP1 = passval[1];
					HOST[HOSTCOUNT].UP2 = passval[2];
					HOSTCOUNT++;
				} else if (passtype.compare("HOSTGROUP") == 0) {
					HOSTGROUP[HOSTGROUPCOUNT].NAME = passname;
					HOSTGROUP[HOSTGROUPCOUNT].VALUE = passval[0];
					if (passname.compare(NAGROUP) == 0) {
						NAPASS = passval[0];
					}
					HOSTGROUPCOUNT++;
				}

			}
		}
	}
	passfile.close();
	
	//debug msglvl > 4
	if (msglvl > 4) {
		int n;
		//print userlogin and userpass
		cout << "Userlogin: " << USERLOGIN << " Userpass: " << USERPASS[0] << " Userpass2: " << USERPASS[1] << endl;
		//print all host/hostgroup associations
		for (n=0; n<HOSTCOUNT; n++) {
			cout << "Hostname: " << HOST[n].NAME << " Hostgroup: " << HOST[n].VALUE << " Userpass: " << HOST[n].UP1 << " Userpass2: " << HOST[n].UP2 << endl;
		}
		//print all hostgroup/password associations
		for (n=0; n<HOSTGROUPCOUNT; n++) {
			cout << "Hostgroup: " << HOSTGROUP[n].NAME << " Password: " << HOSTGROUP[n].VALUE << endl;
		}
	}

	//debug msgslvl > 0
	if (msglvl > 0) {
		cout << "Hostgroup Count: " << HOSTGROUPCOUNT << " Host Count: " << HOSTCOUNT << endl;
	}
	
	return 0;
}

int write_expect(int logintype) {
	int x;
	string Rpasswd;
	string expect_file = RSHOME+"/"+RSEXPECT;
	
	//Determine root group based on hostname specified
	for (x=0; x<HOSTCOUNT; x++) {
		if (HOST[x].NAME.compare(LOGINHOST) == 0) {
			LOGINHOSTGROUP=HOST[x].VALUE;
			if (HOST[x].UP1.compare("") != 0) {
				USERPASS[0] = HOST[x].UP1;
			}
			if (HOST[x].UP2.compare("") != 0) {
				USERPASS[1] = HOST[x].UP2;
			}
		}
	}
	
	//Determine root password based on above root group name
	for (x=0; x<HOSTGROUPCOUNT; x++) {
		if (HOSTGROUP[x].NAME.compare(LOGINHOSTGROUP) == 0) {
			Rpasswd=HOSTGROUP[x].VALUE;
		}
	}
	//debug msglvl > 0
	if (msglvl > 0) cout << "Writing expect data to file: " << expect_file << endl;

	//open expect script file for writing
	ofstream xfile;
	xfile.open ( expect_file.data() );
	if (xfile.is_open() == 0) {
		cout << "Unable to open expect file!" << endl;
		return(-1);
	}
	if (USERPASS[1].compare("") == 0) {
		USE_AUTOPASS = 0;
		USE_PASSREVERT = 0;
	}
	//Begin writing expect code to the script
	xfile << "set timeout 30" << endl;
	xfile << "set hostID " << LOGINHOST << endl;
	xfile << "set KH 0" << endl;
	xfile << "set KA 0" << endl;
	xfile << "set NP 0" << endl;
	xfile << "set OP 0" << endl;

	// Check what kind of login we're doing and set username and password
	if (logintype == 2) {
		xfile << "set Mlogin root" << endl;
		xfile << "set Mpasswd " << NAPASS << endl;
		//debug msglvl > 1
		if (msglvl > 1) cout << "Network Appliance Login method enabled!" << endl;
		Rpasswd = NAPASS;
	} else {
		xfile << "set Mlogin " << USERLOGIN << endl;
		xfile << "set Mpasswd " << USERPASS[0] << endl;
		if (USE_AUTOPASS == 1) {
			xfile << "set Npasswd " << USERPASS[1] << endl;
		}
	}
	
	if (Rpasswd.compare("") == 0) {
		cout << "Warning: No associated root distribution group found for " << LOGINHOST << endl;
	} else {
		xfile << "set Rpasswd " << Rpasswd << endl;
		xfile << endl;
	}
	
	// Check what kind of login we're doing and set switch user login bit
	if (logintype == 1) {
		xfile << "set DOSU 1" << endl;
	} else {
		xfile << "set DOSU 0" << endl;
	}
	
	//finish the rest of the expect login script
	xfile << endl;
	xfile << "#### LOGIN PROCESS BEGINS HERE ####" << endl;
	xfile << "#KH = Known Host" << endl;
	xfile << "#KA = Key Authorization" << endl;
	xfile << "#NP = Setting New Password" << endl;
	xfile << "#OP = Reverting to Original Password" << endl;
	xfile << "log_user " << VERBOSE_LOGIN << endl;
	xfile << "spawn -noecho " << SSHBIN << " -l $Mlogin $hostID" << endl;
	xfile << "expect {" << endl;
	xfile << "\ttimeout { interact }" << endl;
	xfile << "\t\"(yes/no)?\" { send \"yes\r\"; set KH 1 }" << endl;
	xfile << "\t\"ssword:\" { sleep .1; send \"$Mpasswd\r\" }" << endl;
	xfile << "\t\"passphrase\" { sleep .1; send \"$Mpasswd\r\" }" << endl;
	xfile << "\t\"$ \" { set KA 1; send \"\r\" }" << endl;
	xfile << "}" << endl;
	xfile << endl;
	xfile << "if $KH {" << endl;
	xfile << "\texpect {" << endl;
	xfile << "\t\ttimeout { interact }" << endl;
	xfile << "\t\t\"ssword:\" { sleep .1; send \"$Mpasswd\r\" }" << endl;
	xfile << "\t\t\"passphrase\" { sleep .1; send \"$Mpasswd\r\" }" << endl;
	xfile << "\t\t\"> \" { set KA 1; send \"\r\" }" << endl;
	xfile << "\t\t\"$ \" { set KA 1; send \"\r\" }" << endl;
	xfile << "\t}" << endl;
	xfile << "}" << endl;
	xfile << endl;

	if (USE_AUTOPASS == 1) {
		xfile << "expect {" << endl;
		xfile << "\ttimeout { interact }" << endl;
		xfile << "\t\"UNIX password:\" { set NP 1; sleep .1; send \"$Mpasswd\r\" }" << endl; // Linux
		xfile << "\t\" login password\" { set NP 1 }" << endl;					   // Solaris
		xfile << "\t\"\'s password:\" { set OP 1; sleep .1; send \"$Npasswd\r\" }" << endl;  // Bad Password, try secondary
		xfile << "\t\"$ \" { send \"\r\" }" << endl;
		xfile << "\t\"> \" { send \"\r\" }" << endl; // For Netapps
		xfile << "}" << endl;
		xfile << endl;
		xfile << "if $NP {" << endl;
		xfile << "\texpect {" << endl;
		xfile << "\t\ttimeout { interact }" << endl;
		xfile << "\t\t\"New Password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // Solaris
		xfile << "\t\t\"New password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // Linux
		xfile << "\t\t\"New UNIX password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // New Linux
		xfile << "\t}" << endl;
		xfile << "\texpect {" << endl;
		xfile << "\t\ttimeout { interact }" << endl;
		xfile << "\t\t\"Re-enter new Password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // Solaris
		xfile << "\t\t\"Retype new password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // Linux
		xfile << "\t\t\"Retype new UNIX password:\" { sleep .1; send \"$Npasswd\r\" }" << endl; // New Linux
		xfile << "\t}" << endl;
		xfile << "\texpect {" << endl;
		xfile << "\t\ttimeout { interact }" << endl;
		xfile << "\t\t\"$ \" { send \"exit\r\"}" << endl; //Solaris
		xfile << "\t\t\" tokens updated successfully.\"" << endl; //Linux
		xfile << "\t}" << endl;
		xfile << "interact" << endl; // Wait for password change completion, exit code 99
		xfile << "exit 99" << endl;
		xfile << "}" << endl;
		xfile << endl;
	}
		
	xfile << "if $DOSU {" << endl;
	xfile << "\texpect {" << endl;
	xfile << "\t\ttimeout { send \"exec su -\r\" }" << endl;
	xfile << "\t\t\"ssword:\" { sleep .1; send \"$Mpasswd\r\" }" << endl;
	xfile << "\t\t\"> \" { send \"exec su -\r\" }" << endl;
	xfile << "\t\t\"$ \" { send \"exec su -\r\" }" << endl;
	xfile << "\t}" << endl;
	xfile << endl;
	xfile << "\texpect {" << endl;
	xfile << "\t\ttimeout { interact }" << endl;
	xfile << "\t\t\"Password:\" { sleep .1; send \"$Rpasswd\r\" }" << endl;
	xfile << "\t}" << endl;
	if (USE_AUTOPASS == 1 && USE_PASSREVERT == 1) {
		xfile << "\tif $OP {" << endl;
		xfile << "\t\texpect {" << endl;
		xfile << "\t\t\ttimeout { interact }" << endl;
		xfile << "\t\t\t\"# \" { send \"passwd $Mlogin\r\" }" << endl; // Expecting root prompt
		xfile << "\t\t}" << endl;
		xfile << "\t\texpect {" << endl;
		xfile << "\t\t\ttimeout { interact }" << endl;
		xfile << "\t\t\t\"New Password:\" { sleep .1; send \"$Mpasswd\r\" }" << endl; // Solaris
		xfile << "\t\t\t\"New password:\" { sleep .1; send \"$Mpasswd\r\" }" << endl; // Linux
		xfile << "\t\t\t\"New UNIX password:\" { sleep .1; send \"$Mpasswd\r\" } " << endl; // New Linux prompt
		xfile << "\t\t}" << endl;
		xfile << "\t\texpect {" << endl;
		xfile << "\t\t\ttimeout { interact }" << endl;
		xfile << "\t\t\t\"Re-enter new Password:\" { sleep .1; send \"$Mpasswd\r\" }" << endl; // Solaris
		xfile << "\t\t\t\"Retype new password:\" { sleep .1; send \"$Mpasswd\r\" }" << endl;  // Linux
		xfile << "\t\t\t\"Retype new UNIX password:\" { sleep .1; send \"$Mpasswd\r\" }" << endl; // New Linux prompt
		xfile << "\t\t}" << endl;
		xfile << "\t}" << endl;
	}

	xfile << "}" << endl;
	xfile << endl;
	xfile << "interact" << endl;

	xfile.close();
	return 0;
}

int runscript() {
	int err=0;
	string runfile=RSHOME+"/"+RSEXPECT;
	string runcmd=EXPECTBIN+" "+runfile;
	
	if (debug > 0) {
		//debug is enabled, just display the run command to the screen, don't actually run it.
		cout << runcmd << endl;
	} else {
		//debug is not enabled, attempt to run command
		err=system( runcmd.data() );
		// system call multiplys return code by 256
		if (err > 0) {
			err = err / 256;
		}
		//check for system call return code
		if (err == 99) {
			cout << endl << "!!! Your Password expired, it has been changed !!!" << endl;
			cout << "Attempting to log back in" << endl;
			err=system( runcmd.data() ) ;
			// system call multiplys return code by 256
			if (err > 0) {
				err = err / 256;
				cout << "System call exited Abnormally. Code: " << err << endl;
				if (debug == 0) remove(runfile.data());
				return(err);
			}
		} else if (err == 0) { 
			if (msglvl > 3) cout << "System call successful" << endl;
		} else {
			cout << "System call exited Abnormally. Code: " << err << endl;
			if (debug == 0) remove(runfile.data());
			return(err);
		}
	}
	//delete expect script if not debugging and return successful
	//debug msglvl > 0
	if (debug == 0) {
		if (msglvl > 0) cout << "Removing expect data file: " << runfile << endl;
		remove(runfile.data());

	}
	return 0;
}


int checkgpg() {
	string GPGCMD=GPGBIN;
	string password_filename=RSHOME;
	string encpassword_filename=RSHOME;
	int err;
	int gpgerr;
	
	if (USE_GPG == 1) {
		//Configuration has GPG encryption turned on
		encpassword_filename.append("/");
		encpassword_filename.append(RSENCPASS);
		password_filename.append("/.");
		password_filename.append(RSPASS);
		password_filename.append(".tmp");
		GPGCMD.append(" -q -o ");
		GPGCMD.append(password_filename);
		GPGCMD.append(" --decrypt ");
		GPGCMD.append(encpassword_filename);

		//debug msglvl > 0
		if (msglvl > 0) cout << "Decrypting: " << encpassword_filename << endl;
		// Run GPG Decryption program
		gpgerr = system(GPGCMD.data());
		
		// If the decryption failed, return. Otherwise call readpass
		if (gpgerr == 0) {
			if (msglvl > 0) cout << "Reading Passwords from: " << password_filename << endl;
			err = read_pass(password_filename);
			if (err != 0) {
				cout << "Error reading password file!" << endl;
				return(err);
			}
		} else {
			cout << "Error decrypting password file: " << encpassword_filename << endl;
			return(gpgerr);
		}

		// Cleanup CLEAR TEXT Password file
		//debug msglvl > 0
		if (msglvl > 0) cout << "Removing Cleatext Password file: " << password_filename << endl;
		remove(password_filename.data());

	} else {
		// Configuration has GPG encryption turned off
		password_filename.append("/");
		password_filename.append(RSPASS);
		if (msglvl > 0) cout << "Reading Passwords from: " << password_filename << endl;		
		err = read_pass(password_filename.data());
		if (err != 0) {
			cout << "Error reading password file!" << endl;
			return(err);
		}
	}

	return 0;
}

int distro_VZHG(char *distro_filename) {
	string distroline;
	string distroname;
	string distroval;
	size_t delpos[2];
	size_t length;
	int r=0;
	int n;
	
	//open distribution file
	ifstream distro;
	distro.open ( distro_filename );
	//check if the distro file is open, otherwise return
	if (distro.is_open() == 0) {
		cout << "Unable to open distro file: " << distro_filename << endl;
		return(-1);
	}
	
	//check the length of the distro file, then return to the beginning to prepare for reading
	distro.seekg(0,ios::end);
	int rlength = distro.tellg();
	distro.clear();
	distro.seekg(0);
	
	//debug msglvl > 0
	if (msglvl > 0) cout << "Reading Root Distribution List: " << distro_filename << endl;

	//Read distro file line by line looking for groupnames and passwords
	while (distro.tellg() < rlength) {
		getline(distro, distroline);
		//debug msglvl > 5
		if (msglvl > 5) cout << "tellg: " << distro.tellg() << " Record: " << r << endl;
		if (distroline.compare(0,4,"====") == 0) {
			//New root group, increase counter and save root group name
			r++;
			delpos[0] = distroline.find(":");
			delpos[1] = distroline.find("=",delpos[0]);
			if (delpos[0] != string::npos && delpos[1] != string::npos) {
				length=delpos[1]-delpos[0]-3;
				DFHOSTGROUP[r].NAME = distroline.substr(delpos[0]+2, length);
				//debug msglvl > 3
				if (msglvl > 3) cout << "Root Group: " << DFHOSTGROUP[r].NAME << endl;
			}
		} else if (distroline.compare(0,4,"Root") == 0) {
			//root password designation found
			delpos[0] = distroline.find(">");
			delpos[1] = distroline.find("(", delpos[0]);
			if (delpos[0] != string::npos && delpos[1] != string::npos) {
				length=delpos[1]-delpos[0]-3;
				DFHOSTGROUP[r].VALUE = distroline.substr(delpos[0]+2, length);
				//debug msglvl > 3
				if (msglvl > 3) cout << "Root Password: " << DFHOSTGROUP[r].VALUE << endl;
			}
		}
	}
	
	distro.close();
	// Copy HOST struct to DFHOST
	for (n=0; n<MAX_HOSTS; n++) {
		DFHOST[n].NAME = HOST[n].NAME;
		DFHOST[n].VALUE = HOST[n].VALUE;
		DFHOST[n].UP1 = HOST[n].UP1;
		DFHOST[n].UP2 = HOST[n].UP2;
	}
	
	//debug msglvl > 1
	if (msglvl > 2) {
		for (n=1; n<MAX_HOSTGROUPS; n++) {
			if (DFHOSTGROUP[n].NAME.compare("") != 0) {
				cout << "Group: " << DFHOSTGROUP[n].NAME << " Value: " << DFHOSTGROUP[n].VALUE << endl;
			}
		}
	}

	return 0;
}

int distro_VZ(char *distro_filename) {
	string distroline;
	string distroname;
	string distroval;
	size_t delpos[4];
	size_t length;
	int r=0;
	int n;
	int x=0;
	//open distribution file
	ifstream distro;
	distro.open ( distro_filename );
	//check if the distro file is open, otherwise return
	if (distro.is_open() == 0) {
		cout << "Unable to open distro file: " << distro_filename << endl;
		return(-1);
	} 
	
	//check the length of the distro file, then return to the beginning to prepare for reading
	distro.seekg(0,ios::end);
	int rlength = distro.tellg();
	distro.clear();
	distro.seekg(0);
	
	//debug msglvl > 0
	if (msglvl > 0) cout << "Reading Root Distribution List: " << distro_filename << endl;

	//Read distro file line by line looking for groupnames and passwords
	while (distro.tellg() < rlength) {
		getline(distro, distroline);
		//debug msglvl > 5
		if (msglvl > 5) cout << "tellg: " << distro.tellg() << " Record: " << r << endl;
		if (distroline.compare(0,4,"====") == 0) {
			//New root group, increase counter and save root group name
			r++;
			delpos[0] = distroline.find(":");
			delpos[1] = distroline.find("=",delpos[0]);
			if (delpos[0] != string::npos && delpos[1] != string::npos) {
				length=delpos[1]-delpos[0]-3;
				DFHOSTGROUP[r].NAME = distroline.substr(delpos[0]+2, length);
				//debug msglvl > 3
				if (msglvl > 3) cout << "Root Group: " << DFHOSTGROUP[r].NAME << endl;
			}
		} else if (distroline.compare(0,6,"HOSTS:") == 0) {
			// Host list designation found
			int lpos=7;
			int hlength = 0;
			
			while (lpos < distroline.length()) {
				delpos[0] = distroline.find(" ", lpos);
				delpos[1] = distroline.find(" ", delpos[0]+1);
				if (delpos[0] == string::npos) {
					lpos = distroline.length();
				} else if (delpos[1] == string::npos) {
					hlength = distroline.length() - delpos[0] - 1;
					lpos = distroline.length();
					//Check for CRLF line termination
					string lastchar = distroline.substr(distroline.length()-1,1);
					if (lastchar.compare("\r") == 0) {
						hlength--;
					}
					//throw into struct
					DFHOST[x].NAME = distroline.substr(delpos[0]+1, hlength);
					DFHOST[x].VALUE = DFHOSTGROUP[r].NAME;
					if (msglvl > 5) cout << "hlength: " << hlength << " lpos: " << lpos << " delpos-0: " << delpos[0] << " delpos-1: " << delpos[1] << endl;
					if (msglvl > 4) cout << "DFHOST Name: " << DFHOST[x].NAME << " DFHOST Value: " << DFHOST[x].VALUE << endl;
					x++;
				} else {
					hlength = delpos[1] - delpos[0] - 1;
					lpos = delpos[1];
					//throw into struct
					DFHOST[x].NAME = distroline.substr(delpos[0]+1, hlength);
					DFHOST[x].VALUE = DFHOSTGROUP[r].NAME;
					if (msglvl > 5) cout << "hlength: " << hlength << " lpos: " << lpos << " delpos-0: " << delpos[0] << " delpos-1: " << delpos[1] << endl;
					if (msglvl > 4) cout << "DFHOST Name: " << DFHOST[x].NAME << " DFHOST Value: " << DFHOST[x].VALUE << endl;
					x++;
				}
			}
				
		} else if (distroline.compare(0,4,"Root") == 0) {
			//root password designation found
			delpos[0] = distroline.find(">");
			delpos[1] = distroline.find("(", delpos[0]);
			if (delpos[0] != string::npos && delpos[1] != string::npos) {
				length=delpos[1]-delpos[0]-3;
				DFHOSTGROUP[r].VALUE = distroline.substr(delpos[0]+2, length);
				//debug msglvl > 3
				if (msglvl > 3) cout << "Root Password: " << DFHOSTGROUP[r].VALUE << endl;
			}
		}
	}
	
	distro.close();
	//debug msglvl > 1
	if (msglvl > 2) {
		for (n=1; n<MAX_HOSTGROUPS; n++) {
			if (DFHOSTGROUP[n].NAME.compare("") != 0) {
				cout << "Group: " << DFHOSTGROUP[n].NAME << " Value: " << DFHOSTGROUP[n].VALUE << endl;
			}
		}
		for (n=1; n<MAX_HOSTS; n++) {
			if (DFHOST[n].NAME.compare("") != 0) {
				cout << "Host: " << DFHOST[n].NAME << " Value: " << DFHOST[n].VALUE << " UP1: " << DFHOST[n].UP1 << " UP2: " << DFHOST[n].UP2 << endl;
			}
		}
	}

	return 0;
}


int parse_distro(string distro_type, char *distro_filename) {
	int perr = 0;
	string pconfirm="N";
	
	if (distro_type.compare("VZHG") == 0) {
		perr = distro_VZHG(distro_filename);
	} else if (distro_type.compare("VZ") == 0) {
		cout << "Warning: This will overwrite your current HOST configuration." << endl << "Are you sure you want to continue? [y/N]: ";
		cin >> pconfirm;
		if (pconfirm.compare(0,1,"n") == 0) {
				cout << "User aborted import" << endl;
				return -2;
		} else if (pconfirm.compare(0,1,"N") == 0) {
				cout << "User aborted import" << endl;
				return -2;
		} else if (pconfirm.compare(0,1,"y") == 0) {
				perr = distro_VZ(distro_filename);
		} else if (pconfirm.compare(0,1,"Y") == 0) {
				perr = distro_VZ(distro_filename);
		} else {
				cout << "User aborted import" << endl;
				return -2;
		}
	} else {
		cout << "Unknown distribution type!" << endl;
		return -1;
	}

	return perr;
}


int write_passfile() {
	int n;
	string password_filename=RSHOME+"/"+RSPASS;

	cout << "Writing password file: " << password_filename << endl;
	//open password file
	ofstream passfile;
	passfile.open( password_filename.data(), ios::trunc );
	if (passfile.is_open() == 0) {
		cout << "Unable to open password file!" << endl;
		return -1;
	}

	//password file is open, write password data
	passfile << "### This is a commented line and will be skipped, blank lines will also" << endl;
	passfile << "### be skipped." << endl;
	passfile << "USERLOGIN:" << USERLOGIN << endl;
	passfile << "USERPASS:" << USERPASS[0] << endl;
	if (USERPASS[1].compare("") != 0) {
		passfile << "USERPASS2:" << USERPASS[1] << endl;
	} else {
		passfile << "#USERPASS2:the^password^2" << endl;
	}
	passfile << "####### Security Root Distro #######" << endl;
	passfile << "## Syntax for Hostgroups:" << endl;
	passfile << "## HOSTGROUP:<groupname>:<root password>" << endl;
	//Write hostgroups to file
	for (n=1; n<MAX_HOSTGROUPS; n++) {
		if (DFHOSTGROUP[n].NAME.compare("") != 0) {
			passfile << "HOSTGROUP:" << DFHOSTGROUP[n].NAME << ":" << DFHOSTGROUP[n].VALUE << endl;
		}
	}
	
	passfile << "#########################################" << endl;
	passfile << "### Host to Hostgroup Associations" << endl;
	passfile << "### (preserved thru root distro imports)" << endl;
	passfile << "## Syntax for Hosts:" << endl;
	passfile << "## HOST:<hostname>:<hostgroup>:[userpass]:[userpass2]" << endl;
	//Write Hosts to file
	for (n=0; n<MAX_HOSTS; n++) {
		if (DFHOST[n].NAME.compare("") != 0) {
			passfile << "HOST:" << DFHOST[n].NAME << ":" << DFHOST[n].VALUE << ":" << DFHOST[n].UP1 << ":" << DFHOST[n].UP2 << endl;
		}
	}
	
	passfile.close();
	return 0;
}

int encrypt_pass() {
	int gpgerr = 0;
	string password_filename=RSHOME+"/"+RSPASS;
	string GPGCMD=GPGBIN+" --encrypt "+password_filename;

	cout << "Encrypting: " << password_filename << endl;
	if (debug > 0) {
		//debug - just print the gpg command
		cout << GPGCMD << endl;
		return 0;
	} else {
		//normal mode - run gpg command
		gpgerr = system(GPGCMD.data());
		if (gpgerr == 0) {
			//gpg encryption successful, remove clear text file
			remove( password_filename.data() );
			return 0;
		} else {
			cout << "Encryption failed!" << endl;
			return gpgerr;
		}
	}

	return gpgerr;
}

void usagehelp(int ECODE) {
	cout << "Usage:\t" << PNAME << " [options] [hostname]" << endl;
	cout << "\t" << PNAME << " [-v] [-h]" << endl;
	cout << "\t" << PNAME << " [-c <file>] [-r] [-V] <hostname>" << endl;
	cout << "\t" << PNAME << " [-c <file>] [-R] [-V] <hostname>" << endl;
	cout << "\t" << PNAME << " [-c <file>] [-n] [-V] <hostname>" << endl;
	cout << "\t" << PNAME << " [-c <file>] [-i <file>] [-t <type>] [-e]" << endl;
	cout << endl;
	cout << "Options:" << endl;
	cout << "  -h\t\t Display help" << endl;
	cout << "  -v\t\t Display version" << endl;
	cout << "  -c <file>\t Use configuration file (default: ~/.rslogin/rslogin.cfg)" << endl;
	cout << "  -r\t\t Login to server as root" << endl;
	cout << "  -R\t\t Login to server as a normal user" << endl;
	cout << "  -n\t\t Special login, Server is a Network Appliance" << endl;
	cout << "  -V\t\t Toggle Verbose Login" << endl;
	cout << "  -e\t\t Encrypt rslogin password file" << endl;
	cout << "  -i <file>\t Import root distribution" << endl;
	cout << "  -t <type>\t Set import file type" << endl;
	cout << endl;
	cout << "Import File Types:" << endl;
	cout << "  VZ - Verizon Internal Root Distribution, Hosts and Hostgroups" << endl;
	cout << "  VZHG - Verizon Internal Root Distribution, Hostgroups only (default)" << endl;
	cout << endl;
	exit(ECODE);
}

void version() {
	cout << PNAME << ", version " << VERSION << "-" << DEVSTATE << endl; 
}

int main (int argc, char *argv[]) {
	int c;
	int err;
	int logintype=0;
	int logintypeset=0;
	int pderr[2];
	int pdimport=0;
	int pdencrypt=0;
	int toggle_verbose=0;
	char *host=NULL;
	char *user_conf;
	char *config_filename=NULL;
	char *import_filename=NULL;
	string import_type="VZHG";
	struct hostent *hp;
	
	
	while ((c = getopt(argc, argv, "c:t:i:hVvedrRnm")) != -1) {
		switch (c) {
			case 'h':
				usagehelp(0);
			case 'v':
				version();
				exit(0);
			case 'V':
				toggle_verbose=1;
				break;
			case 'c':
				config_filename = optarg;
				break;
			case 'd':
				debug++;
				break;
			case 'm':
				msglvl++;
				break;
			case 'r':
				if (logintypeset == 0) {
					logintype=1;
					logintypeset=1;
					break;
				} else {
					usagehelp(-2);
				}
			case 'R':
				if (logintypeset == 0) {
					logintype=0;
					logintypeset=1;
					break;
				} else {
					usagehelp(-2);
				}
			case 'n':
				if (logintypeset == 0) {
					logintype=2;
					logintypeset=1;
					break;
				} else {
					usagehelp(-2);
				}
			case 't':
				import_type = optarg;
				break;
			case 'i':
				pdimport = 1;
				import_filename = optarg;
				break;
			case 'e':
				pdencrypt = 1;
				break;
			default:
				exit(-99);
		}
	}
	
	
	host = argv[argc-1];
	if (strncmp(host,"-",1) == 0) {
		host = NULL;
	} else if (strncmp(host,".",1) == 0) {
		host = NULL;
	} else if (strncmp(host,"/",1) == 0) {
		host = NULL;
	} else {
		//debug msglvl > 0
		if (msglvl > 0) cout << "Host: " << host << endl;
	}
	//if we're not importing or encrypting a password file, make sure hostname isn't blank
	if (pdimport == 0 && pdencrypt == 0) {
		if (host != NULL) {
			LOGINHOST=host;
		} else {
			usagehelp(-3);
		}
	}

	// Determine location of user configuration file
	USERHOME = gethome();
	user_conf = new char [USERHOME.size()+21];
	strcpy(user_conf, USERHOME.c_str());
	strcat(user_conf, ".rslogin/rslogin.cfg");
	if (msglvl > 4) {
		cout << "USERHOME: " << USERHOME << endl;
		cout << "user_conf: " << user_conf << endl;
	}

	// If a configuration file wasn't specified on the command line, set default to user config
	if (config_filename == NULL) {
		config_filename = new char [strlen(user_conf)];
		strcpy (config_filename, user_conf);
		if (msglvl > 4) cout << "Using Default user configuration: " << config_filename << endl;
	}
	//Read in the configuration file
	err = read_config(config_filename);
	if (err != 0) {
		exit(err);
	}
	// If the AUTOROOT option is set in the config, and no command line override was provided, set for root login
	if (USE_AUTOROOT == 1) {
		if (logintypeset == 0) {
			logintype = 1;
		}
	}
	// Check for Toggling Verbose Mode
	if (toggle_verbose == 1) {
		if (VERBOSE_LOGIN == 1) {
			VERBOSE_LOGIN = bool(0);
		} else if (VERBOSE_LOGIN == 0) {
			VERBOSE_LOGIN = bool(1);
		}
	}
	// Decrypt a GPG encrypted password file is enabled, otherwise just read in the password file
	err = checkgpg();
	if (err != 0) {
		exit (err);
	}
	
	// Check pdimport and pdencrypt, if either are true, we're not logging into a server
	if (pdimport == 1 || pdencrypt == 1) {
		if (pdimport == 1) {
			//pdimport true, we're importing a root distro
			pderr[0] = parse_distro(import_type, import_filename);
			if (pderr[0] == 0) {
				//write parsed distro to password file
				pderr[0] = write_passfile();
				if (pderr[0] != 0) exit(pderr[0]);
			} else {
				cout << "Unable to Parse Distribution file!" << endl;
			}
		}
		
		if (pdencrypt == 1 && pderr[0] == 0) {
			//pdencrypt true, we're encrypting password file
			pderr[1] = encrypt_pass();
		}
		
		//determine overall error code and exit
		pderr[2] = pderr[0] + pderr[1];
		exit(pderr[2]);
	}

	// Write expect script to file based on configuration and command line options
	err = write_expect(logintype);
	if (err != 0) {
		exit(err);
	}

	// validate hostname provided on command-line
	if (host != NULL) {
		hp = (struct hostent *) gethostbyname(host);
		if (hp == NULL) {
			cout << "Unable to resolve name: " << host << endl;
			if (debug <= 0) { exit(-1); }
		}
	} else {
		version();
		usagehelp(-1);
	}
	//Execute expect script
	err = runscript();


	return err;
}
