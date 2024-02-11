#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <utmp.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <math.h>
#include <unistd.h>

/**
*@brief
*@param
*@param
*@return
*/

// define structs
typedef struct { // for memory usage
    double physUsed;
    double physTotal;
    double virtUsed;
    double virtTotal;
    
} MemData;

typedef struct { //for cpuusage
    // 0     1     2       3      4       5    6        7      8     9
    // user  nice  system  idle   iowait  irq  softirq  steal  guest guest_nice
    long cpuuse[10]; //idle is cpu[3]
    long sum;
    int error;
    double usage;
}CPUData;




//PARSE COMMAND LINE ARGUMENTS
int parseCLA(int argc, char **argv, int toprint[4],int *Nptr, int *tdelayptr);

//PRINT AND INPUT INFORMATION 
int printRunningParam(int N, int tdelay); //input info and print
int updateMemory(MemData *memdata, int i); //take samples updates
int printMemory(MemData* memdata, int i, int N, int seq,int graphics); //calculate & print
int printUsers(); //take samples and print
int printCores(); //input info and print
void printCPUUsage(CPUData prev, CPUData curr,CPUData *cpudata,int i,int graphics); //calculate and print
int printSysInfo(); // input info and print
CPUData updateCPU(); //input info

//helper functions
void copyCPUData(CPUData *dest, CPUData *src);
double convertbytes(long bytes, int unit); //convert amount bytes in unit unit(where unit = #of bytes per one unit of bytes) to GB
int parseInt(char *line);
void convertSec(long sec, int time[4]);

/** function name: double convertbytes(long bytes, int unit)
*@brief Convert the long bytes in unit unit to GB
*@param unit The #of bytes per one unit of amount bytes
*@param bytes Amount bytes to be converted
*@return double result, the amount bytes in GB
*/
double convertbytes(long bytes, int unit){
    double result = (double)(bytes/(1024.0*1024*1024/unit));
    return result;
}

/**
*@brief take sample of memory data using struct sysinfo defined by <sys/sysinfo.h> 
        update it to the array of MemData of size N(total # of samples) at position i
*@param memdata array that contains all the samples of MemData(phys.used/Tot -- Virtual Used/ToT)
*@param i current sample of MemData (ie. we are taking the ith sample of MemData)
*@return -1 if error in acessing sysinfoData, 0, o/w
*Note: This function is only called in printMemory(MemData* memdata, int i, int N, int seq)
*/
int updateMemory(MemData* memdata, int i){
    //call sysinfo() fnc from <sys/sysinfo.h> & check error
    struct sysinfo sysinfoData;
    if(sysinfo(&sysinfoData)==-1){
        printf("Error in updateMemory(MemData* memdata, int i) --Error in calling sysinfo");
        return -1; // will return -1 to printMemory which will return -1 to main which will terminate the program
    };

    //update memdata at index i, using the parameter info in sysinfo struct, sysinfoData
    int unit = sysinfoData.mem_unit;
    memdata[i].physUsed = convertbytes(sysinfoData.totalram - sysinfoData.freeram,unit); 
    memdata[i].physTotal = convertbytes(sysinfoData.totalram,unit);
    memdata[i].virtTotal = convertbytes(sysinfoData.totalswap+sysinfoData.totalram,unit); 
    memdata[i].virtUsed = memdata[i].virtTotal-convertbytes(sysinfoData.freeram+sysinfoData.freeswap,unit);
    return 0; //return if successful
}

/**
*@brief Takes a sample of /proc/stat, Initializes a CPUData(parameters cpuuse[10],sum), Open and Read Data from file /proc/stat into that CPUData
*@return a CPUData c, contains info about user, nice, system and idles time spent, as well as the total sum, and whether or not an error has occured
*/
CPUData updateCPU(){
    //declare vars
    CPUData c;
    FILE *statptr;
    char line [100];
    //reset vars
    for(int i = 0;i<10;i++){
        c.cpuuse[i] = 0;
    }
    c.sum = 0;
    //openfile
    statptr = fopen("/proc/stat","r");
    if(statptr == NULL){ //error check
        printf("Error opening /proc/stat in function CPUData updateCPU()\n");
        c.error = 1;
        return c;
    }
    //read first line of file into parameter of CPUData (long array cpuuse )
    fscanf(statptr,"%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",line,
            &c.cpuuse[0], &c.cpuuse[1], &c.cpuuse[2], &c.cpuuse[3],&c.cpuuse[4],
            &c.cpuuse[5], &c.cpuuse[6], &c.cpuuse[7], &c.cpuuse[8],&c.cpuuse[9]);
    //get and store the total timespent into CPUData c
    for(int i = 0;i<10;i++){
        c.sum+=c.cpuuse[i];

    }
    
    fclose(statptr);//close pointer
    c.error = 0;//indicate no error
    return c;
}

/**
*@brief helper functions that takes two CPUData ptrs, dest and src and copies all data from src into dest
*@param dest CPUData ptr to be copied into
*@param src CPUData ptr to be copied from
*/
void copyCPUData(CPUData *dest, CPUData *src){
    for(int i = 0;i<10;i++){
        dest->cpuuse[i] = src->cpuuse[i];
    }
    dest->sum = src->sum;
    dest->error = src->error;
    dest->usage = src->usage;
}

/**
*@brief takes a long sec, and convert into day,hour,minutem second form and storing int array time
*@param sec long that reps. total secs to be converted
*@param time[4] int arr to store days, hours, mins, and secs
*/
void convertSec(long sec, int time[4]){
    time[0] = (int)(((sec/60)/60)/24); //days
    time[1] = (int)(((sec/60)/60)%24); //hours
    time[2] = (int)((sec/60)%60); // minutes
    time[3] = (int)(sec%60); // seconds
}

/** 
*@brief convert string line to integer
*@param line char ptr to be converted
*@return the integer contained in line
*/
int parseInt(char *line){
    //init parsed
    int parsed = 0;

    for(int i = 0; i<strlen(line);i++){
        //check if current char is digit
        if(line[i]<'0' || line[i]>'9') return -1;
        int new_digit = 1; // init digit value (ie. 1s, 10s, 100s digit)
        //loop to give the correct digit value
        for(int j = 0;j<i;j++){
            new_digit = new_digit*10;
        }
        //parse char and *by digit and add it to parse
        parsed = parsed + (line[strlen(line)-1-i]-48)*new_digit;
    }
    return parsed;
}

/**
*@brief print Running Parameters of the functions (Nbr of samples, and time delay)
*       and the amount of memory the program is using in kB
*@param N int that represents the number of samples 
*@param tdelay int that reps the number of seconds between each sample taken
*
*/
int printRunningParam(int N, int tdelay){
    struct rusage r_usage;
    if(getrusage(RUSAGE_SELF,&r_usage)){
        printf("Error Getting Memory Usage\n");
    }
    printf("Nbr of samples: %d -- every %d secs\n",N, tdelay);
    printf(" Mermory usage: %ld kilobytes\n",r_usage.ru_maxrss);
    printf("---------------------------------------\n");
}

/**
*@brief Takes an array of type MemData, the current sample (i), and whether or not 
*       to print graphics or sequentially and prints the current total and used phys mem
*       and virtual mem used.
*       If sequential, it will print a blank line for every previous data sample
*       Otherwise, it will print all previous samples
*       For graphics, each "block" (#, or :) represents total increase of 0.1 from the first
*       data sample taken
*@param memdata array that stores all previous samples of memory
*@param i integer that reps. the current sample(ie. we are printing the ith sample)
*@param N total number of samples to be taken
*@param seq integer used as boolean --> 1, if we are to print sequentially, 0 o/w
*@param graphics int used as boolean --> 1, if we are to print graphics, 0 o/w
*@return -1, if there is an error in accessing memory, 0 if the function is successful.
*/
int printMemory(MemData* memdata, int i, int N, int seq, int graphics){
    //print header
    printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
    if(updateMemory(memdata, i)==-1){ //get memory and store in memdata[i] and check for error
        return -1; 
    }
    
    //print Memory Data, the ith iteration prints the current sample
    for(int j = 0; j<i+1;j++){

        //print all previous samples if not sequential, and print the current sample on the ith iter
        if(!seq||j==i){
            printf("%.2f GB / %.2f GB -- %.2f GB / %.2f GB\t",memdata[j].physUsed, memdata[j].physTotal, memdata[j].virtUsed,memdata[j].virtTotal);
        }
        
        //print graphics
        if(graphics&&!seq||((j==i)&&graphics)){
            int positive = 1;
            double prevUsed = 0;
            double currUsed = 0;

            //get phys. usuage of the jth sample and the j-1th sample
            currUsed = memdata[j].physUsed;
            if(j==0)prevUsed = currUsed;
            else prevUsed = memdata[j-1].physUsed;
            
            int blocks = (int)round((currUsed-prevUsed)/0.01); //calc # of blocks
            
            if(currUsed-prevUsed<0)positive = 0; //store sign
            else positive = 1;
            printf("|"); // divider line
            //print blocks
            for(int k =0;k<fabs(blocks);k++){
                if(positive)printf(":");
                else printf("#");
            }
            //print end of blocks
            if((positive&&(blocks>0))||(blocks==0)&&(!positive))printf("@"); //if positive and more than 1 block or 0 block negative
            else if((positive)&&(blocks==0))printf("o"); // if positive and 0 blocks
            else printf("*"); //negative and blocks>0
            printf(" %.2f (%.2f)",currUsed-prevUsed,memdata[j].virtUsed); // print usage 
            
        }
        printf("\n"); //go to next line
    }
    //print blank lines for samples not yet taken 
    for(int j = 0;j<N-i-1;j++){
            printf("\n");
    }
    
    printf("---------------------------------------\n"); //section seperator
    return 0;
}

/**
*@brief get information from sysinfo (uptime) and utsname to print the details
*       regarding system details (system name, machine name,OS version etc.)
*@return -1 if there is an acessing sysinfo, 0 o/w
*/
int printSysInfo(){
    //getting the structs sysinfo and uname and error checking
    struct sysinfo sysinfoData;
    if(sysinfo(&sysinfoData)==-1){
        printf("Error Acessing sysinfo() in function: int printSysInfo\n");
        return -1;
    }
    struct utsname unameData;
    if(uname(&unameData)==-1){
        printf("Error Acessing uname() (from utsname library) in function: int printSysInfo\n");
        return -1;
    }

    //print system information
    //printf("---------------------------------------\n");
    printf("### System Information ###\n");
    printf("System Name = %s\n", unameData.sysname);
    printf("Machine Name = %s\n", unameData.nodename);
    printf("Version = %s\n", unameData.version);
    printf("Release = %s\n",unameData.release);
    printf("Architecture = %s\n", unameData.machine);
    
    // print uptime
    int time[4]; // declare var to store converted time
    convertSec(sysinfoData.uptime, time); //convert total seconds into day hour min second format
    printf("System running since last reboot: %d days %d:%d:%d", time[0],time[1],time[2],time[3]); // print with days
    printf(" (%d:%d:%d)\n",(time[0]*24+time[1]),time[2], time[3]); // print with day amount included in hours
    printf("---------------------------------------\n");
    return 0;

}

/**
*@brief calculate and print the CPU %usage, and for both graphical and non graphics versions 
*Note: CPU Usage is calculated by taking the change in total time - the change in idle time
*as referenced from this website https://www.idnt.net/en-US/kb/941772
*      # of bars in graphics is calculated by taking the current %usage and printing a | 
*      for every 0.2%
*All values are stored in longs to prevent type errors
*@param prev CPUData containing previous snapshot of /proc/stat
*@param curr CPUData containing current snapshot of /proc/stat
*@param cpudata CPUData array containing all snapshots of CPUData
*@param i int rep. current iteration of samples
*@param graphics tells whether or not to print graphics
*/

void printCPUUsage(CPUData prev, CPUData curr,CPUData *cpudata,int i,int graphics){

    //calculate percent usage
    long sumchng = curr.sum - prev.sum;
    long idlechng = curr.cpuuse[3]-prev.cpuuse[3];
    long used = sumchng-idlechng;
    double percent = round(100*100*used/sumchng)/100.0;

    //store usage in current CPUData
    curr.usage = percent;
    copyCPUData(&cpudata[i+1],&curr); // copy usage data to the one at i+1
                                    //the first snapshot of CPUData has a usage of 0%
    printf("total cpu use = %.2f%%\n", percent); //print current CPU use
    if(graphics){ 
        //for loop prints all previous %usages samples as well is the current one
        for(int j =1;j<i+2;j++){
            int bars = (int)round(cpudata[j].usage/0.2); //caculate # of bars to be printed
            printf("\t\t");
            for(int k = 0;k<bars;k++){ //for loop to print the bars
                printf("|");
            }
            printf("%.2f\n",cpudata[j].usage); //display the % usage rep. by the bars
        }
    }
    
}

/**
*@brief read number of processors and print number of cores 
* @return -1 if error getting #of cores from sysconf, 0 o/w
*/
 int printCores(){
    //get # of processors from sysconf lib/
    int numCores = sysconf(_SC_NPROCESSORS_CONF);
    if(numCores<=0){ //chec for errors
        printf("Error: Failed to get # of cores\n");
        return -1;
    }
    printf("Number of cores: %d\n",numCores); //print #of cores
    return 0;
}

/**
*@brief read user info data(username, tty and session) from utmp and print the username 
*NOTE: this can only accomdate up to 20 unique users
*@return total # of user sessions connected
*/
int printUsers(){
    //init cnts
    int userlinecnt = 0;
    int uniqUserCnt = 0;
    
    struct utmp *userinfo;
    userinfo = malloc(sizeof(struct utmp)); //assign memory for utmp
    setutent(); //"open"
    //declare
    char username[33];
    char tty[33];
    char host[257];

    userinfo = getutent(); //read first user info
    printf("### Sessions/users ###\n");
    //loop until userinfo is null
    while(userinfo!=NULL){
        if(userinfo->ut_type == USER_PROCESS){ //chec if its a user
            //copy username, tty and host info into the declared vars, and terminate the names
            strncpy(username, userinfo->ut_user, 32); 
            username[32] = '\0';
            strncpy(tty, userinfo->ut_line, 32);
            tty[32] = '\0';
            strncpy(host, userinfo->ut_host, 256);
            host[256] = '\0';

            //print user, tty and host info
            printf("%s      %s", username, tty);
            printf(" (%s)\n",host);
            
            userlinecnt++;
        }
        
        userinfo = getutent(); //get new user from utmp
    }
    endutent(); //"close" utmp file
    free(userinfo);

    printf("---------------------------------------\n");
    return userlinecnt; //return total # of users connected
    
}


/**
*@brief parse command line arguments to update N(number of samples), tdelay (time delay), and read all flags
*       and update int toprint[4] to keep track of which flags have been triggered and what to print during the session
*@param argc int # of command line args
*@param char **argv string array containing all CLA arguments
*@param toprint[4] int arr that keeps track what to print
//toprint[4]
    //index  description
    // 0 --> if 1, print graphics
    // 1 --> if 1, system should be printed
    // 2 --> if 1, user should be printed
    // 3 --> if 1, sequential
*@param int *Nptr pointer, location where number of samples are stored
*@param int *tdelayptr pointer, location where time delay should be stored
*@return 0 if invalid CLAs, 1 o/w
*/

int parseCLA(int argc, char **argv, int toprint[4],int *Nptr, int *tdelayptr){
    int N = 10;
    int tdelay = 1;
    //start looking at CLAs at index i, since argv[0] is the executable file
    for(int i = 1; i<argc; i++){
        if(strstr(argv[i], "--")==NULL){ // no -- detected, positional arguments (samples, tdelay)
            //parse # of samples
            N = parseInt(argv[i]);
            *Nptr = N; //update N in main
            
            //check if only one positional argument at the end of the command line call
            if(argc<=i+1){
                  printf("Error: invalid positional sample/time delay input\n You Entered: %s\n", argv[i]);
                  return 0; //terminate parsing
            }
            i++;
            // parse # of samples
            tdelay = parseInt(argv[i]);
            *tdelayptr = tdelay; //update tdelay in main
            if(N == -1 || tdelay == -1){ // true if either pos args have non-digits in them
                printf("Error: invalid positional sample/time delay input\n You Entered: %s %s\n", argv[i-1], argv[i]);
                return 0; //terminate parsing
            }


        }
        
        else{ // -- detected
            if(strcmp(argv[i], "--system")==0){
                toprint[1] = 1; //toggle toprint so that system  usage is printed
            }
            else if(strstr(argv[i], "--samples=")!=NULL){ //udate N in main(samples) if --samples=N is called
                char *num = strchr(argv[i], '='); //take substring of '=N'
                N = parseInt(num+1); //parse string starting after '='
                *Nptr = N; 
                if(N == -1)return 0; //terminate parsing if non-digit entered
                
            }
            else if(strstr(argv[i], "--tdelay=")!=NULL){ // update tdelay in main if --tdelay=tdelay is called
                char *num1 = strchr(argv[i], '='); //take substring of '=tdelay'
                tdelay = parseInt(num1+1);
                *tdelayptr = tdelay;
                if(tdelay == -1)return 0; //terminate parsing if non-digit entered
            }
            
            else if(strcmp(argv[i], "--user")==0){ //toggle toprint so that user usage is printed
                toprint[2] = 1;
                  
            }
            else if(strcmp(argv[i], "--sequential")==0){ //toggle toprint so that everything is sequentially printed
            
                toprint[3] = 1;
            }
            else if(strcmp(argv[i],"--graphics")==0){ //toggle toprint so that everything is graphically printed
                toprint[0] = 1;
                //printf("Unfortunately, graphics has not been implemented in this monitoring tool\n");
                //return 0;
            }
            
            else{//-- detected but the word after isn't one that we've defined
                printf("Error: --flag format incorrect/not found\nYou Entered: %s\n",argv[i]); 
                return 0;
            }
            

        }
    }
    return 1; // successfully parsed
}


/**
*@brief main function
*       take samples in this function
*@param argc # of arguments
*@param char **argv string array of all CLAs
*@return 0 to terminate
*/
int main(int argc, char **argv){
    //toprint[4]
    //index  description
    // 0 --> if 1, print graphics
    // 1 --> if 1, system should be printed
    // 2 --> if 1, user should be printed
    // 3 --> if 1, sequential
    
    int toprint[] = {0,0,0,0}; //initalize everything as 0
    int N = 10; // number of samples
    int tdelay = 1; //time delay
        
    //parse CLA, terminate if there are errors in the CLA
    if(parseCLA(argc, argv, toprint, &N, &tdelay)==0)return 0;

    //declare array and allocate memory of type Memdata to store all samples of memory(phys. and virt. usage)
    //declare array and allocate CPUData of type CPUData to store all snapshots of cpu usage
    MemData *memdata = (MemData *)malloc(N*sizeof(MemData));
    CPUData *cpudata = (CPUData *)malloc((N+1)*sizeof(CPUData));
    //if neither --system, nor --user flags are used, toggle toprint to print both
    if(toprint[1] == 0 && toprint[2] ==0){
        toprint[1] = 1;
        toprint[2] = 1;
    }
    
    //declare CPUDatas needed (prev, curr)
    CPUData curr = updateCPU(); //take initial sample of CPUData
    curr.usage = 0; //set first snapshot usage to 0
    CPUData prev; 
    copyCPUData(&prev,&curr);//set prev to to the initial snapshot of CPUData
    if(prev.error||curr.error)return 0; //terminate if error in updating CPUData
    
    //first iteration
    if(!toprint[3]){ //clear screen if not seq
        printf("\033[2J"); //clear screen
        printf("\033[0;0H"); //move cursor to top left corner
    }
    printRunningParam(N,tdelay);
    
    if(toprint[1]){//check if print sysusage
        if(printMemory(memdata, 0, N,toprint[3],toprint[0])==-1)return 0; //call+errorcheck memory, store 1st sample in memdata[0]
    }
    if(toprint[2]) printUsers(); //check if print user usage, and print
    if(toprint[1]){//check if print sysusage for cpu data (cores+usage)
        if(printCores()==-1)return 0;//print+errorcheck cores
        sleep(tdelay); //pause
        curr = updateCPU();//collect second sample to find CPU usage
        printCPUUsage(prev, curr,cpudata,0,toprint[0]); // calc and print first usage
    }

    //beginning print samples 2nd samples to Nth Sample
    for(int i =1;i<N;i++){
        sleep(tdelay); 
        //clear screen 
        if(!toprint[3]){
            printf("\033[2J");
            printf("\033[0;0H");
        }
        //if seq print new current sample num
        if(toprint[3])printf(">>> iteration %d\n",i+1);
        else printRunningParam(N,tdelay); //print running param --> will look at mem self-utilization at every iteration
        if(toprint[1]){ //true if toprint sys usage
            printMemory(memdata, i, N,toprint[3],toprint[0]); //print memory
        }
        
        if(toprint[2]) printUsers(); //true if toprint user usage
        
        if(toprint[1]){ //print cpu usage (core and %usage)
            if(printCores()==-1)return 0; //print cores
            printCPUUsage(prev,curr,cpudata,i-1,toprint[0]); //print previous CPU Usage
            copyCPUData(&prev,&curr); //make previous current
            sleep(tdelay); //timedelay
            curr = updateCPU(); //take new snapshot of CPU usage
            if(prev.error||curr.error)return 0; //terminate if something went wrong
            //move cursor to the line beginning of the line with "Number of Cores"
            if(toprint[0])printf("\033[%dF",1+i);
            else printf("\033[1F"); 
            
            fflush(stdout);
            //print new CPUusage
            printCPUUsage(prev,curr,cpudata,i,toprint[0]);

        }
               
        
    }
    
    if(!(toprint[2]&&!toprint[1]))printf("---------------------------------------\n");
    printSysInfo();
    
    
    //free allocated arrays
    free(memdata);
    free(cpudata);


}