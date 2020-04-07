#include <errno.h>
#include <signal.h>
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <netdb.h>
#include <inttypes.h>
#include "interface.h"
#include "receiver.h"  /* Include the header here, to obtain the function declaration */

#define BUFSIZE 1024

//for behavior of ethercat slave you have to understand DSP 402, but we thing EPOS3 firmware guide is really helpful try to undrstand EPOS3 firmware guide and DSP402. Below address we get from DSP402 and EPOS3 firmware
#define CONTROL_WORD 0x6040, 0x00 // RW, uint16
#define SHUTDOWN 0x0006
#define SWITCH_ON 0x0007
#define DISABLE_VOLTAGE 0x0000
#define QUICK_STOP 0x0002
#define DISABLE_OPERATION 0x0007
#define ENABLE_OPERATION 0x000F
#define FAULT_RESET 0x0080
#define STATUS_WORD 0x6041, 0x00 // R, uint16
#define INPUT_STATUS 0x60FD, 0x0 // R, uint16
#define ALARM_STATUS 0x603F, 0x0 // R, uint16
#define NOT_READY_TO_SWITCH_ON 0x0000
#define SWITCH_ON_DISABLED 0x0040
#define READY_TO_SWITCH_ON 0x0021
#define SWITCH_ON_ENABLED 0x0023
#define OPERATION_ENABLED 0x0027
#define QUICK_STOP_ACTIVE 0x0007
#define FAULT_REACTION_ACTIVE 0x000F
#define FAULT 0x0008
//We have to set the mode of the drive according to our requirment
//to get the values from slaves we should know the index and subindex
#define ACTUAL_POSITION 0x6064, 0x00
#define TARGET_POSITION 0x607A, 0x00 // RW, int32
#define OPERATION_MODE 0x6060, 0x00 // RW, uint8
#define PROFILE_POSITION_MODE 0x01
#define PROFILE_VELOCITY_MODE 0x03
#define PROFILE_TORQUE_MODE 0x04
#define HOMING_MODE 0x06
#define INTERPOLATED_POSITION_MODE 0x07

#define POSITION_RANGE 0x607B, 0x00 // R, uint8
#define MIN_POSITION_RANGE_LIMIT 0x607B, 0x01 // RW, int32
#define MAX_POSITION_RANGE_LIMIT 0x607B, 0x02 // RW, int32
#define SOFTWARE_POSITION_LIMIT 0x607D, 0x00
#define MAX_PROFILE_VELOCITY 0x607F, 0x00
#define PROFILE_VELOCITY 0x6081, 0x00
#define TARGET_VELOCITY 0x6080, 0x00
  #define PROFILE_ACCELERATION 0x6083, 0x00
#define PROFILE_DECELERATION 0x6084, 0x00
#define QUICK_STOP_DECELERATION 0x6085, 0x00
#define MOTION_PROFILE_TYPE 0x6086, 0x00
#define DRIVE_MODE 0x60F2, 0x00
#define MAX_TARGET_VELOCITY   94967294
//#define MAX_TARGET_VELOCITY 5000000
#define RATIO 2500
/*************************************************************************
***/
//header file comes with IgH ethercat master, you can get help for any function used in our code from this header file(functions related to ethercat only), you can find this header file in /opt/etherlab/include
#include "ecrt.h"
//for getting time you have to use below parameters
/*************************************************************************
***/
// Application parameters
#define FREQUENCY 100
#define CLOCK_TO_USE CLOCK_REALTIME
#define MEASURE_TIMING
/*************************************************************************
***/
#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)
#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
(B).tv_nsec - (A).tv_nsec)
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)
/*************************************************************************
***/
//assgment variables for ethercat master, slaves and domain etc
// EtherCAT
static ec_master_t *drive_master[2];
static ec_master_t *master = NULL;
static ec_master_t *master0 = NULL;
static ec_master_t *master1 = NULL;
static ec_master_state_t master_state = {};
static ec_domain_t *domain0 = NULL;
static ec_domain_state_t domain_state = {};
static ec_slave_config_t *sc_epos3 = NULL;
static ec_slave_config_state_t sc_epos3_state = {};

int id;
char *position;
int step_mode = 0;
int UNDER_MOTION = 0;

//Thread const
pthread_t tid[3];
pthread_t status_tid;
pthread_t local_tid, halt_tid, local_tid2 = 0;
// Timer
static unsigned int sig_alarms = 0;
static unsigned int user_alarms = 0;
// process data
static uint8_t *domain0_output = NULL;
//ethrcat slave position, vendor and product ID
//
// offsets for PDO entries
/*************************************************************************
***/
//* Vendor ID:       0x0000066f
//* Product code:    0x535300a1
#define SLAVE_DRIVE_0 0, 0
#define MAXON_EPOS3 0x0000066f, 0x535300a1
static unsigned int off_epos3_cntlwd;
static unsigned int off_epos3_tpos;
static unsigned int off_epos3_off_pos;
static unsigned int off_epos3_off_vel;
static unsigned int off_epos3_off_toq;
static unsigned int off_epos3_moo;
static unsigned int off_epos3_dof;
static unsigned int off_epos3_tpf;
static unsigned int off_epos3_status;
static unsigned int off_epos3_pos_val;
static unsigned int off_epos3_vel_val;
static unsigned int off_epos3_tps;
//domain registration, you have to register domain so that you can send and receive PDO data
static unsigned int off_epos3_toq_val;
static unsigned int off_epos3_mood;
static unsigned int off_epos3_dif;
static unsigned int off_epos3_tpp1pv;
static unsigned int off_epos3_tpp1nv;
static unsigned int off_error_code;
static unsigned int off_act_value;
static unsigned int off_digital_inputs;
static unsigned int off_profile_velocity;
static unsigned int off_profile_acceleration;
static unsigned int off_profile_deceleration;
static unsigned char *prev_stat;
static int ECS_ENABLED = 0;
static int HOMING_DIR = 1;
static int DRIVE_DIR = 1;
static int CL_DL = 1;
static int STEP_MODE = 0;
static int FIN_SIGNAL = 0;
static int ZERO_REF = 0;
static int STOP_PROGRAM = 0;
static int CLAMP_ERROR = 0;
static int DECLAMP_ERROR = 0;


//{SLAVE_DRIVE_0, MAXON_EPOS3, 0x2078, 1, &off_epos3_dof}, // U16 17
//{SLAVE_DRIVE_0, MAXON_EPOS3, 0x2071, 1, &off_epos3_dif}, //

const static ec_pdo_entry_reg_t domain0_regs[] = {
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x6040, 0, &off_epos3_cntlwd}, // U16 0
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x6060, 0, &off_epos3_moo}, // S8 16
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x60b8, 0, &off_epos3_tpf}, // U16 19
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x6041, 0, &off_epos3_status}, // 21
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x6061, 0, &off_epos3_mood}, //
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x6064, 0, &off_epos3_pos_val}, // 23
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x60b9, 0, &off_epos3_tps}, //
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x60ba, 0, &off_epos3_tpp1pv}, //
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x607a, 0, &off_epos3_tpos},
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x603f, 0, &off_error_code},
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x60f4, 0, &off_act_value},
  {SLAVE_DRIVE_0, MAXON_EPOS3, 0x60fd, 0, &off_digital_inputs},
  {}
};
//{SLAVE_DRIVE_0, MAXON_EPOS3, 0x6081, 0, &off_profile_velocity},
//{SLAVE_DRIVE_0, MAXON_EPOS3, 0x6083, 0, &off_profile_acceleration},
//{SLAVE_DRIVE_0, MAXON_EPOS3, 0x6083, 0, &off_profile_deceleration},
static unsigned int counter = 0;
static unsigned int blink = 0;
static unsigned int fault_flag = 0;
static unsigned int state_of_the_drive = 0; static unsigned int sync_ref_counter = 0;
const struct timespec cycletime = {0, PERIOD_NS};

char ** res  = NULL;
int count = 0;
unsigned int init_pulse = 0;
int START_EXECUTION = 0;
int prev_postion = 999;
int ACTIVE_DRIVE_COUNT = 0;
int ACTIVE_DRIVE = 0;
int errorCode = 0;
int JOG_RPM = 1200000;
int TIMING = 500000;
void *requester;
int ecssignal=0;
int COUNTER = 300;
int EMG_ACTIVE = 0;

typedef struct {
    //Or whatever information that you need
    int drive_id;
    char position[20];
    int mode;
} position_params;


/* Master 0, Slave 0, "MDDHT3530BA1"
 * Vendor ID:       0x0000066f
 * Product code:    0x535300a1
 * Revision number: 0x00010000
 */
 ec_pdo_entry_info_t slave_0_pdo_entries[] = {
     {0x6040, 0x00, 16}, /* Controlword */
     {0x6060, 0x00, 8}, /* Modes of operation */
     {0x607a, 0x00, 32}, /* Target position */
     {0x60b8, 0x00, 16}, /* Touch probe function */
     {0x603f, 0x00, 16}, /* Error code */
     {0x6041, 0x00, 16}, /* Statusword */
     {0x6061, 0x00, 8}, /* Modes of operation display */
     {0x6064, 0x00, 32}, /* Position actual value */
     {0x60b9, 0x00, 16}, /* Touch probe status */
     {0x60ba, 0x00, 32}, /* Touch probe pos1 pos value */
     {0x60f4, 0x00, 32}, /* Following error actual value */
     {0x60fd, 0x00, 32}, /* Digital inputs */

 };

 ec_pdo_info_t slave_0_pdos[] = {
     {0x1600, 4, slave_0_pdo_entries + 0}, /* Receive PDO mapping 1 */
     {0x1a00, 8, slave_0_pdo_entries + 4}, /* Transmit PDO mapping 1 */
 };

 ec_sync_info_t slave_0_syncs[] = {
     {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
     {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
     {2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_ENABLE},
     {3, EC_DIR_INPUT, 1, slave_0_pdos + 1, EC_WD_DISABLE},
     {0xff}
 };

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
  struct timespec result;
  if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
    result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
    result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
  } else {
    result.tv_sec = time1.tv_sec + time2.tv_sec;
    result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
  }
  return result;
}

//we need to check the state of domain , state of the domain should be equal to 2 when it exchanged the data
int check_domain_state(ec_domain_t *domain)
{
  ec_domain_state_t ds;
  ecrt_domain_state(domain, &ds);
  if (ds.working_counter != domain_state.working_counter)
    //printf("Domain: WC %u.\n", ds.working_counter);
  if (ds.wc_state != domain_state.wc_state)
    //printf("Domain: State %u.\n", ds.wc_state);
  domain_state = ds;
  if(domain_state.wc_state == 2) {
    return 0;
  }
  else {
    return -1;
  }
}


//check the state of the master that it is in operation, link in up and no of slaves responding
void check_master_state(void)
{
  ec_master_state_t ms;
  ecrt_master_state(master, &ms);
  if (ms.slaves_responding != master_state.slaves_responding)
    //printf("%u slave(s).\n", ms.slaves_responding);
  if (ms.al_states != master_state.al_states)
    //printf("AL states: 0x%02X.\n", ms.al_states);
  if (ms.link_up != master_state.link_up)
    //printf("Link is %s.\n", ms.link_up ? "up" : "down");
  master_state = ms;
}



/*************************************************************************

****/

//check slave state it is in operational state or not, PDO data can not be transfer it slave is not in operational state
void check_slave_config_states(void)
{
  ec_slave_config_state_t s;
  ecrt_slave_config_state(sc_epos3, &s);
  if (s.al_state != sc_epos3_state.al_state)
    printf("EPOS3 slave 0 State 0x%02X.\n", s.al_state);
  if (s.online != sc_epos3_state.online)
    printf("EPOS3 slave 0: %s.\n", s.online ? "online" : "offline");
  if (s.operational != sc_epos3_state.operational)
    printf("EPOS3 slave 0: %soperational.\n",
  s.operational ? "" : "Not ");
  sc_epos3_state = s;
}

unsigned long int binaryToLongValue(int *binary_value){
  unsigned long int v = 0;
  int i =0 ;
  //Let's convert only 32 bits and not 64 bits
  for(i=0;i<32;i++){
    v += binary_value[i]*pow(2,i);
  }
  return v;
}

int * longToBinary(unsigned long int decimal)
{
  int binaryNumber[100],i=1,j,k=0;
  int len;
  unsigned long int quotient;
  int* b = malloc(sizeof(int) * 64);
  int* c = malloc(sizeof(int) * 64);
  quotient = decimal;

  while(quotient!=0){
       binaryNumber[i++]= quotient % 2;
       quotient = quotient / 2;
  }
  len = i-1;
  for(j = i -1 ;j> 0;j--){
    //b[k++] = binaryNumber[j];
    k++;
    b[len-k] = binaryNumber[j];
  }
  for(j=i; j<64; j++){
    b[j] = 0;
  }
  return b;
}


int * decimalToBinary(int decimal)
{
  int binaryNumber[100],i=1,j,k=0;
  int quotient, len;
  int* b = malloc(sizeof(int) * 64);
  quotient = decimal;

  while(quotient!=0){
       binaryNumber[i++]= quotient % 2;
       quotient = quotient / 2;
  }
  len = i-1;
  for(j = i -1 ;j> 0;j--){
    b[k++] = binaryNumber[j];
  }
  for(j=i; j<64; j++){
    b[j] = 0;
  }
  return b;
}

void halt(void *args){
  unsigned short controlWord = 0x0005;
  size_t data_size = sizeof(controlWord);
  int errorCode = 0;
  uint32_t abortCode = 0;
  position_params *pp = args;
  //printf("Drive Id is --> %d, Position is --> %s\n",pp->drive_id, pp->position);
  master0 = drive_master[pp->drive_id];
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, data_size, &abortCode);
  //printf("Halted Jog Mode\n");
}

void Jog(int id, int type, int immediate){
  master0 = drive_master[id];
  unsigned short controlWord = 0x005f;
  printf("Inside Jog\n");
  if(type == 0){
    if(immediate == 0){
        controlWord = 0x001f;
    }else{
        controlWord = 0x003f;
    }
  }else{
    if(immediate == 1){
        controlWord = 0x007f;
    }
  }
  printf("controlWord value is -> %d\n",controlWord);
  //unsigned short controlWord = 0x001f;
  int pollingDelay = 2000;
  size_t data_size = sizeof(controlWord);
  int errorCode = 0;
  int i =0, k;
  uint32_t abortCode = 0;
  unsigned short statusWord = 0xFFFF;
  size_t resultSize = 0;
  int *resp;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, data_size, &abortCode);
  //printf("Abort Message During JOG: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);
  printf("Jog Triggered\n");
  printf("Error code after Jog --> %d", errorCode);
  //for(i=0; i<500; i++){
  while(1){
    if((errorCode = ecrt_master_sdo_upload(master0, 0, STATUS_WORD, (unsigned char *)&statusWord, sizeof(statusWord), &resultSize, &abortCode)) < 0) {
      //printf("Abort Message : %x\n", abortCode);
      return;
    }
    resp = decimalToBinary(statusWord);
    if(resp[10] == 1 && resp[12] == 1){
      //printf("Target reached...\n");
      //return;
      controlWord = 0x004f;
      /*if(type == 0){
          controlWord = 0x000f;
      }*/
      errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, data_size, &abortCode);
    }
    if(resp[10] == 1 && resp[12] == 0){
      printf("Target reached...\n");
      // usleep(5000);
      //sendInterrupt(1);
      //zmq_send(requester, "TARGET", 6, 0);
      //printf("Completed Sending ZMQ");
      //zmq_send(requester, "TARGET", 6, 0);
      //return;
      break;
    }
  }
  //usleep(pollingDelay*1000);
  //kill(getpid(),SIGKILL);
//}

}

void PowerOn(int id){
  unsigned short controlWord;
  unsigned short statusWord;
  int errorCode = 0;
  uint32_t abortCode = 0;
  unsigned char period = 10;
  unsigned char operationMode = 8;
  unsigned long maxFlowingError = 5000000;
  uint16_t value = 0x0006;
  master0 = drive_master[id];

  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);
  usleep(100000);
  //printf("Drive init complete\n");
  value = 0x0007;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);
  usleep(100000);
  //printf("Drive magnetize complete\n");
  value = 0x004f;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);
  usleep(100000);
  //printf("Drive Power on complete\n");
}

void Configure(int id){
  unsigned short controlWord;
  unsigned short statusWord;
  int errorCode = 0;
  uint32_t abortCode = 0;
  unsigned char period = 10;
  unsigned char operationMode = 8;
  unsigned long maxFlowingError = 5000000;
  uint8_t cw_value = 0x1;
  uint16_t drive_mode = 2; // 0 , 1 -> Absolute, 2 -> Incremental
  size_t data_size = sizeof(cw_value);
  master0 = drive_master[id];
  //sudo ethercat download 0x6060 0 1
  errorCode = ecrt_master_sdo_download(master0, 0, OPERATION_MODE, (unsigned char*)&cw_value, data_size, &abortCode);
  //printf("Abort Message OPERATION MODE: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  //sudo ethercat download 0x607a 0 5000000
  maxFlowingError = 100000;
  //maxFlowingError = 100000;
  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, TARGET_POSITION, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  //sudo ethercat download 0x6081 0 2000000 - PROFILE_VELOCITY
  maxFlowingError = 1000000;
  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, PROFILE_VELOCITY, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  //sudo ethercat download 0x6083 0 5000000 - PROFILE_ACCELERATION
  maxFlowingError = 100000000;
  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, PROFILE_ACCELERATION, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  //sudo ethercat download 0x6084 0 2500000 - PROFILE_DECELERATION
  //maxFlowingError = 500000;
  maxFlowingError = 100000000;

  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, PROFILE_DECELERATION, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);
  //Set Drive to Absolute MODE
  drive_mode = 2;
  errorCode = ecrt_master_sdo_download(master0, 0, DRIVE_MODE, (unsigned char *)&drive_mode, sizeof(drive_mode), &abortCode);

  EMG_ACTIVE = 0;
}

unsigned short GetStatus(unsigned int position)
{
  unsigned short statusWord = 0xFFFF;
  int errorCode = 0;
  size_t resultSize = 0;
  uint32_t abortCode = 0;
  if((errorCode = ecrt_master_sdo_upload(master, position, STATUS_WORD, (unsigned char *)&statusWord, 2, &resultSize, &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return statusWord;
  }

  if((statusWord & 0x4F) == 0x00) {
    //printf("%d : Not ready to switch on.\n", position);
    statusWord = NOT_READY_TO_SWITCH_ON;
  }

  else if((statusWord & 0x4F) == 0x40) {
    //printf("%d : Switch on disabled.\n", position);
    statusWord = SWITCH_ON_DISABLED;
  }

  else if((statusWord & 0x6F) == 0x21) {
    //printf("%d : Ready to switch on.\n", position);
    statusWord = READY_TO_SWITCH_ON;
  }
  else if((statusWord & 0x6F) == 0x23) {
    //printf("%d : Switch on enabled.\n", position);
    statusWord = SWITCH_ON_ENABLED;
  }
  else if((statusWord & 0x6F) == 0x27) {
    //printf("%d : Operation enabled.\n", position);
    statusWord = OPERATION_ENABLED;
  }
  else if((statusWord & 0x6F) == 0x07) {
    //printf("%d : Quick stop active.\n", position);
    statusWord = QUICK_STOP_ACTIVE;
  }
  else if((statusWord & 0x4F) == 0x0F) {
    //printf("%d : Fault reaction active.\n", position);
    statusWord = FAULT_REACTION_ACTIVE;
  }
  else if((statusWord & 0x4F) == 0x08) {
    //printf("%d : Fault\n", position);
    statusWord = FAULT;
  }
  return statusWord;
}

void SwitchOffSlave(int id){
  //sudo ethercat download 0x6040 0 0x0007
  master0 = drive_master[id-1];
  uint32_t abortCode = 0;
  int errorCode = 0;
  unsigned int shutdown = 0x0002;
  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&shutdown, sizeof(shutdown), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Shutdown Completed\n");
  return;
}

void getAngle(char *str, float pulses){
    ////printf("Pulses are --> %f\n", pulses);
    int d1 = pulses;
    float f2 = pulses - d1;     // Get fractional part (678.0123 - 678 = 0.0123).
    int d2 = trunc(f2 * 10000);   // Turn into integer (123).
    sprintf (str, "%d.%04d\n", d1, d2);
    ////printf("Angle --> %s", str);
}

int powerOff(int id){
  int errorCode = 0;
  uint32_t abortCode = 0;
  master0 = drive_master[0];

  int n=0;
  int *binary_value;
  //uint16_t value = 0x0007;
  unsigned short value = 0xFFFF;
  size_t resultSize=0;
  //Get all the params..

  errorCode = ecrt_master_sdo_upload(master0, 0, 0x6040,0x00, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
  if(errorCode < 0){
	  return errorCode;
  }
  binary_value = longToBinary(value);
  binary_value[0] = 0;
  binary_value[1] = 1;
  binary_value[2] = 1;
  binary_value[3] = 0;
  binary_value[7] = 0;
  value=binaryToLongValue(binary_value);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);

  //printf("Drive Power off complete-1\n");
  usleep(150000);
  return errorCode;
}

void powerOn(int id){
  int errorCode = 0;
  uint32_t abortCode = 0;
  uint16_t value = 0x000F;
  master0 = drive_master[id-1];
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);
  //printf("Drive Power on complete-1\n");

}

int* readInputSignal(int id){
  unsigned long int v = 0xFFFFFFFF;
  uint32_t abortCode = 0;
  int errorCode = 0;
  size_t resultSize = 0;
  int k;
  int *resp;
  char res[32];
  char out_stat[4];
  master0 = drive_master[id-1];
  ////printf("Inside Read Input Signal\n");
  if(ecrt_master_sdo_upload(master0, 0, INPUT_STATUS, (unsigned char *)&v, sizeof(v), &resultSize, &abortCode) <0 ){
    printf("\n\n\n-----ALERT:: FAILURE------\n\n\n");
  }
  resp = longToBinary(v);
  return resp;
}

int* readAlarms(int id){
  unsigned long int v = 0xFFFFFFFF;
  uint32_t abortCode = 0;
  int errorCode = 0;
  size_t resultSize = 0;
  int k;
  int *resp;
  char res[32];
  master0 = drive_master[id-1];
  ////printf("Inside Read Input Signal\n");
  if(ecrt_master_sdo_upload(master0, 0, ALARM_STATUS, (unsigned char *)&v, sizeof(v), &resultSize, &abortCode) <0 ){
    //printf("\n\n\n-----ALERT:: FAILURE------\n\n\n");
  }
  resp = longToBinary(v);
  return resp;
}

int* readOutputSignal(int id){
  unsigned long int v = 0xFFFFFFFF;
  uint32_t abortCode = 0;
  int errorCode = 0;
  size_t resultSize = 0;
  int k;
  int *resp;
  char res[32];
  char out_stat[4];
  master0 = drive_master[id-1];
  ////printf("Inside Read Input Signal\n");
  if(ecrt_master_sdo_upload(master0, 0, 0x60FE,01, (unsigned char *)&v, sizeof(v), &resultSize, &abortCode) <0 ){
    //printf("\n\n\n-----ALERT:: FAILURE------\n\n\n");
  }
  resp = longToBinary(v);
  return resp;
}



void sendInterrupt(int id){
  printf("Inside Finish Signal..\n");
  if(FIN_SIGNAL == 0){
    printf("Finish Signal is Disabled\n");
    zmq_send(requester, "TARGET", 6, 0);
    zmq_send(requester, "TARGEE", 6, 0);
    printf("Completed Sending ZMQ\n");
    return;
  }
  printf("Finish signal condition check completed");
  master0 = drive_master[id-1];
  int errorCode = 0, n=0;
  uint32_t abortCode = 0;
  int *binary_value;
  int *registry;
  // int emg = registry[23];
  //uint16_t value = 0x0007;
  unsigned long int value = 0xFFFFFFFF;
  size_t resultSize;

  printf("Inside Send Interrupt\n");
  registry = readInputSignal(1);
  // emg = registry[23];
  printf("read input signal complete.., %d", registry[23]);
  if (registry[0] == 1 || registry[1] == 1){
	printf("POT/NOT is enabled and hence not moving %d, %d\n", registry[1], registry[0]);
	return;
  }
  //Do not send interrupt if there are emergency or other alarms..
  printf("Check emergency value\n");

  if(EMG_ACTIVE == 1){
	  EMG_ACTIVE = 0;
	  printf("There is an emergency. Hence not Sending interrupt");
	  return;
  }

  //Get all the params..
  printf("GET ALL PARAMS\n");
  ecrt_master_sdo_upload(master0, 0, 0x60FE,0x01, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
  binary_value = longToBinary(value);
  binary_value[16] = 1;
  //printf("\n");
  value = binaryToLongValue(binary_value);
  //printf("Converted decimal value --> %lu\n",value);

  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x02, (unsigned char *)&value, sizeof(value), &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x01, (unsigned char *)&value, sizeof(value), &abortCode);
  binary_value[16] = 0;
  value = binaryToLongValue(binary_value);
  printf("Sleep 100ms\n");
  usleep(getFinishSignalTiming());
  UNDER_MOTION = 0;
  //errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x02, (unsigned char *)&value, sizeof(value), &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x01, (unsigned char *)&value, sizeof(value), &abortCode);
  printf("Sending Interrupt Signal\n");
  zmq_send(requester, "TARGET", 6, 0);
  printf("Sending TARGET COMPLETE\n");
  usleep(1000);
  zmq_send(requester, "TARGEE", 6, 0);
  printf("Completed Sending ZMQ");
}

void toggleEmergency(int id, int val){
  int errorCode = 0;
  uint32_t abortCode = 0;
  master0 = drive_master[0];

  int n=0;
  int *binary_value;
  //uint16_t value = 0x0007;
  unsigned short value = 0xFFFF;
  size_t resultSize=0;
  //Get all the params..
  if(val == 1)
  {
    ecrt_master_sdo_upload(master0, 0, 0x6040,0x00, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
    binary_value = longToBinary(value);
    binary_value[0] = 1;
    binary_value[1] = 1;
    binary_value[2] = 1;
    binary_value[3] = 0;
    binary_value[7] = 0;
    value=binaryToLongValue(binary_value);
    errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0, (unsigned char *)&value, sizeof(value), &abortCode);
    //printf("Value is %d\n",value);
    //printf("Sending Emergency Signal\n");
  }

  else{
    //printf("remove emergency\n");
    PowerOn(id);
    return;
  }

  //Get all the params..
  /*ecrt_master_sdo_upload(master0, 0, 0x6040,0x00, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
  binary_value = longToBinary(value);
  value = binaryToLongValue(binary_value);
  //printf("Before conversion %lu\n",value);
  binary_value[2] = val;
  //printf("\n");
  value = binaryToLongValue(binary_value);
  //printf("Converted decimal value --> %lu\n",value);*/
}


//void moveToPosition(int id, char *position){
//void moveToPosition(void* pos_param){

int getTimingCounter(){
  if(TIMING == 0){
    printf("Timing counter is 500\n");
    return 500;
  }
  printf("Timing counter is %d\n",TIMING/100000);
  return TIMING/100000;
}

int getFinishSignalTiming(){
    if(TIMING == 0){
        printf("Default timing value of 500ms\n");
        return 500000;
    }
	printf("Timing value is %d\n", TIMING);
    return TIMING;
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}


void moveToPosition(void *args){
  int i=0;
  int sum = 0;
  int x = 0;
  int *registry;
  int counter = 0;
  int sleep_counter = 100;
  struct timeval stop, start;
  uint32_t val;
  uint32_t abortCode = 0;
  int errorCode = 0;
  float elapsed;
  printf("Inside Move to Position --> %d\n", UNDER_MOTION);
  while(counter > 0){
	  if(UNDER_MOTION == 0){
		  break;
	  }
	  counter--;
	  usleep(1000);
  }
  if(UNDER_MOTION == 1){
      printf("Drive is under motion. Hence not moving..");
      return;
  }

  position_params *pp = args;
  printf("Drive Id is --> %d, Position is --> **%s**, Mode --> %d\n",pp->drive_id, pp->position, pp->mode);
  printf("Zerof Ref --> %d\n", ZERO_REF);
  master0 = drive_master[pp->drive_id];
  printf("Converting from string to int\n");
  sum = atoi(pp->position);
  printf("Convertion from string to int complete\n");
  printf("Sum --> %d\n",sum);
  //val = sum * RATIO;
  val = sum;
  //printf("----%d-----\n",val);

  //If ECS is enabled.
  //1) Servo On 2) De-Clamp 3) Wait For Declamp Signal 4) Move
  //5)Clamp 6) Wait for Clamp 8) Servo Off 9) Finish Signal

  //Wait for ECS to be high..
  //Set flag if the drive is idle..
  UNDER_MOTION = 1;
  printf("STOP Program status --> %d\n"+STOP_PROGRAM);
  if(STOP_PROGRAM == 1){
	STOP_PROGRAM = 0;
	UNDER_MOTION = 0;
	return;
  }
  registry = readInputSignal(1);
  if (registry[0] == 1 || registry[1] == 1){
	   printf("POT/NOT is enabled and hence not moving %d, %d\n", registry[1], registry[0]);
	   UNDER_MOTION = 0;
	   return;
  }

  if(pp->mode == 1 && ZERO_REF == 0){
	printf("Mode is 1");
	//printf("ECS Staus --> %d\n",ECS_ENABLED);
	if(ECS_ENABLED == 1 ){
	  printf("ECS is enabled. So wait for external input\n");
	}
	//Got ECS, now Servo On
	powerOn(1);
	printf("Servo On Complete\n");
	//Read Table De-Clamp Signal
	if(CL_DL == 1 && ZERO_REF == 0){
	  printf("Read De-Clamp Signal after Servo On\n");
	  //printf("Waiting fro Declamp Signal\n");
	  // counter = getTimingCounter();
	counter = COUNTER;
	gettimeofday(&start, NULL);
	  while(1){
		counter--;
		gettimeofday(&stop, NULL);
		printf("Start time in ms %lu and stop time %lu\n", stop.tv_usec, start.tv_usec);
		printf("COUNTER -> %d\n",COUNTER);
		elapsed = timedifference_msec(start, stop);
		printf("Difference --> %f\n",elapsed);
		if(elapsed > COUNTER){
		  DECLAMP_ERROR = 1;
		  UNDER_MOTION = 0;
		  return;
		}
		registry = readInputSignal(1);
		printf("%d",registry[21]);
		if(registry[21] == 1){
		  printf("Got De-Clamp Signal. Now next step\n");
		  break;
		}
		usleep(1000);
	  }
	}
  }else{
	powerOn(1);
	printf("Direct Servo On Complete\n");
  }

  //DeClamp Completed. Move Now.
  errorCode = ecrt_master_sdo_download(master0, 0, TARGET_POSITION, (unsigned char *)&val, sizeof(val), &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);
  printf("Starting Jog Now\n");
  Jog(0,1,0);
  printf("Jog Completed Now\n");
  //Read from Alarm registry if emergency is active..
  // registry = readInputSignal(1);
  // if(registry[23] == 1){
	//   printf("There was a reset issued and hence returing..");
	//   EMG_ACTIVE = 0;
	//   UNDER_MOTION = 0;
	//   return;
  // }
  printf("Emergency value check --> %d\n\n",EMG_ACTIVE);
  if(EMG_ACTIVE == 1){
	  printf("There was a reset issued and hence returing..");
	  EMG_ACTIVE = 0;
	  UNDER_MOTION = 0;
	  return;
  }
  printf("Jog Mode is --> %d\n", pp->mode);
  if(pp->mode == 1){
	//printf("Move Completed. Now Servo Off\n");
	//Wait for clamp signal
	if(CL_DL == 1){
	  powerOff(1);
	  if(ZERO_REF == 0){
		printf("Power Off complete. Now Send Finish Signal\n");
		printf("Waiting for Clamp Signal\n");
		// counter = getTimingCounter();
	  	counter = COUNTER;
		gettimeofday(&start, NULL);
		while(1){
		  counter--;
		  gettimeofday(&stop, NULL);
		  printf("Start time in ms %lu and stop time %lu\n", stop.tv_usec, start.tv_usec);
		  printf("COUNTER -> %d\n",COUNTER);
		  elapsed = timedifference_msec(start, stop);
		  printf("Difference --> %f\n",elapsed);
		  if(elapsed > COUNTER){
			CLAMP_ERROR = 1;
			UNDER_MOTION = 0;
			return;
		  }
		  registry = readInputSignal(1);
		  //  //printf("%d",registry[20]);
		  if(registry[20] == 1){
			printf("Got Clamp Signal. Now send finish signal\n");
			break;
		  }
		usleep(1000);
		}
		ZERO_REF = 0;
		zmq_send(requester, "REFCOM", 6, 0);
	  }
	}
	if(errorCode){
	  printf("Error with position completion. Hence not sending finish signal..");
	  UNDER_MOTION = 0;
	  return;
	}
	UNDER_MOTION = 0;
	sendInterrupt(1);
  }
  else{
	if(CL_DL == 1 && pp->mode != 2){
	  printf("Direct Power Off\n");
	  powerOff(1);
	}

	//powerOff(1);
	//printf("Interrupt Send Complete\n");
	//sendInterrupt(1);
	ZERO_REF = 0;
	UNDER_MOTION = 0;
	zmq_send(requester, "REFCOM", 6, 0);
  }
  UNDER_MOTION = 0;
  printf("Resetting UNDER_MOTION --> %d\n",UNDER_MOTION);
}


void moveToNegativePosition(void *args){
	int i=0;
    int sum = 0;
    int x = 0;
    int *registry;
    int counter = 0;
    int sleep_counter = 100;
	struct timeval stop, start;
    uint32_t val;
    uint32_t abortCode = 0;
    int errorCode = 0;
    float elapsed;
    printf("Inside Move to Position --> %d\n", UNDER_MOTION);
    while(counter > 0){
  	  if(UNDER_MOTION == 0){
  		  break;
  	  }
  	  counter--;
  	  usleep(1000);
    }
    if(UNDER_MOTION == 1){
        printf("Drive is under motion. Hence not moving..");
        return;
    }

    position_params *pp = args;
    printf("Drive Id is --> %d, Position is --> **%s**, Mode --> %d\n",pp->drive_id, pp->position, pp->mode);
    printf("Zerof Ref --> %d\n", ZERO_REF);
    master0 = drive_master[pp->drive_id];
    printf("Converting from string to int\n");
    sum = atoi(pp->position);
    printf("Convertion from string to int complete\n");
    printf("Sum --> %d\n",sum);
    //val = sum * RATIO;
	sum = sum * -1;
    val = sum;
    //printf("----%d-----\n",val);

    //If ECS is enabled.
    //1) Servo On 2) De-Clamp 3) Wait For Declamp Signal 4) Move
    //5)Clamp 6) Wait for Clamp 8) Servo Off 9) Finish Signal

    //Wait for ECS to be high..
    //Set flag if the drive is idle..
	UNDER_MOTION = 1;
    printf("STOP Program status --> %d\n"+STOP_PROGRAM);
    if(STOP_PROGRAM == 1){
  	STOP_PROGRAM = 0;
  	UNDER_MOTION = 0;
  	return;
    }
    registry = readInputSignal(1);
    if (registry[0] == 1 || registry[1] == 1){
  	   printf("POT/NOT is enabled and hence not moving %d, %d\n", registry[1], registry[0]);
  	   UNDER_MOTION = 0;
  	   return;
    }

    if(pp->mode == 1 && ZERO_REF == 0){
  	printf("Mode is 1");
  	//printf("ECS Staus --> %d\n",ECS_ENABLED);
  	if(ECS_ENABLED == 1 ){
  	  printf("ECS is enabled. So wait for external input\n");
  	}
  	//Got ECS, now Servo On
  	powerOn(1);
  	printf("Servo On Complete\n");
  	//Read Table De-Clamp Signal
  	if(CL_DL == 1 && ZERO_REF == 0){
  	  printf("Read De-Clamp Signal after Servo On\n");
  	  //printf("Waiting fro Declamp Signal\n");
  	  // counter = getTimingCounter();
  	counter = COUNTER;
  	gettimeofday(&start, NULL);
  	  while(1){
  		counter--;
  		gettimeofday(&stop, NULL);
  		printf("Start time in ms %lu and stop time %lu\n", stop.tv_usec, start.tv_usec);
  		printf("COUNTER -> %d\n",COUNTER);
  		elapsed = timedifference_msec(start, stop);
  		printf("Difference --> %f\n",elapsed);
  		if(elapsed > COUNTER){
  		  DECLAMP_ERROR = 1;
  		  UNDER_MOTION = 0;
  		  return;
  		}
  		registry = readInputSignal(1);
  		printf("%d",registry[21]);
  		if(registry[21] == 1){
  		  printf("Got De-Clamp Signal. Now next step\n");
  		  break;
  		}
  		usleep(1000);
  	  }
  	}
    }else{
  	powerOn(1);
  	printf("Direct Servo On Complete\n");
    }

    //DeClamp Completed. Move Now.
    errorCode = ecrt_master_sdo_download(master0, 0, TARGET_POSITION, (unsigned char *)&val, sizeof(val), &abortCode);
    //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);
    printf("Starting Jog Now\n");
    Jog(0,1,0);
    printf("Jog Completed Now\n");
    //Read from Alarm registry if emergency is active..
    // registry = readInputSignal(1);
    // if(registry[23] == 1){
  	//   printf("There was a reset issued and hence returing..");
  	//   EMG_ACTIVE = 0;
  	//   UNDER_MOTION = 0;
  	//   return;
    // }
    printf("Emergency value check --> %d\n\n",EMG_ACTIVE);
    if(EMG_ACTIVE == 1){
  	  printf("There was a reset issued and hence returing..");
  	  EMG_ACTIVE = 0;
  	  UNDER_MOTION = 0;
  	  return;
    }
    printf("Jog Mode is --> %d\n", pp->mode);
    if(pp->mode == 1){
  	//printf("Move Completed. Now Servo Off\n");
  	//Wait for clamp signal
  	if(CL_DL == 1){
  	  powerOff(1);
  	  if(ZERO_REF == 0){
  		printf("Power Off complete. Now Send Finish Signal\n");
  		printf("Waiting for Clamp Signal\n");
  		// counter = getTimingCounter();
  	  	counter = COUNTER;
  		gettimeofday(&start, NULL);
  		while(1){
  		  counter--;
  		  gettimeofday(&stop, NULL);
  		  printf("Start time in ms %lu and stop time %lu\n", stop.tv_usec, start.tv_usec);
  		  printf("COUNTER -> %d\n",COUNTER);
  		  elapsed = timedifference_msec(start, stop);
  		  printf("Difference --> %f\n",elapsed);
  		  if(elapsed > COUNTER){
  			CLAMP_ERROR = 1;
  			UNDER_MOTION = 0;
  			return;
  		  }
  		  registry = readInputSignal(1);
  		  //  //printf("%d",registry[20]);
  		  if(registry[20] == 1){
  			printf("Got Clamp Signal. Now send finish signal\n");
  			break;
  		  }
  		usleep(1000);
  		}
  		ZERO_REF = 0;
  		zmq_send(requester, "REFCOM", 6, 0);
  	  }
  	}
  	if(errorCode){
  	  printf("Error with position completion. Hence not sending finish signal..");
  	  UNDER_MOTION = 0;
  	  return;
  	}
  	UNDER_MOTION = 0;
  	sendInterrupt(1);
    }
    else{
  	if(CL_DL == 1 && pp->mode != 2){
  	  printf("Direct Power Off\n");
  	  powerOff(1);
  	}

  	//powerOff(1);
  	//printf("Interrupt Send Complete\n");
  	//sendInterrupt(1);
  	ZERO_REF = 0;
  	UNDER_MOTION = 0;
  	zmq_send(requester, "REFCOM", 6, 0);
    }
    UNDER_MOTION = 0;
    printf("Resetting UNDER_MOTION --> %d\n",UNDER_MOTION);
}

void pollStatus(){
    unsigned int stat = 0xFFFF;
    char str[50];
    size_t res_size = 0;
    uint32_t abt_code = 0;
    char snum[32];
    int err = 0;
    float pos;
    char buffer [50];
    char *st1, *str2;
    unsigned int old_pos_1=999;
    unsigned int old_pos_2=999;
    int char_count=0;
	int errorCode = 0;
	int fail_count = 0;

    //char str[50];
    while(1){
      master0 = drive_master[0];
      char_count = 0;
      ////printf("Inside Poll Status\n");
      errorCode = ecrt_master_sdo_upload(master0, 0, 0x6064, 0, (unsigned char *)&stat, sizeof(stat), &res_size, &abt_code);
	  if(errorCode < 0){
		  fail_count++;
		  if(fail_count > 10){
			  printf("Unable to communicate with EtherCAT.. Shutting Down..");
    		  exit(-1);
		  }
	  }
	  fail_count = 0;
      sprintf(str, "%d", stat);
      while(str[char_count] != '\0'){
        char_count++;
      }
      ////printf("Number of chars %d\n",char_count);
      //printf("Position %s\n",str);
      zmq_send(requester, str, char_count, 0);
      /*if(stat != old_pos_1){
        sprintf(str, "%d", stat);
        //printf("Current Position %s\n",str);
        zmq_send(requester, str, 100, 0);
        old_pos_1 = stat;
      }*/
      usleep(1000);
    }
}

void startHoming(int id){
  uint32_t abortCode = 0;
  unsigned short statusWord = 0xFFFF;
  unsigned short controlWord = 0x001f;
  int errorCode = 0;
  uint32_t value;
  size_t resultSize = 0;
  uint8_t cw_value = 0x6;
  uint16_t cw;
  int8_t method = 3;
  int homing_falg = 0;
  int i=0, k=0;
  int *resp;
  size_t data_size = sizeof(controlWord);

  master0 = drive_master[id];

  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6060, 0x00, (unsigned char*)&cw_value, sizeof(cw_value), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Setting Homing Mode Completed\n");
  if(HOMING_DIR == 1){
    value = 3;
  }else{
    value = 5;
  }

  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6098, 0x00, (unsigned char*)&method, sizeof(method), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Setting Homing type Completed\n");
  value = 500000;
  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x609A, 0x00, (unsigned char*)&value, sizeof(value), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Setting Homing Acceleration Completed\n");
  value = 500000;
  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6099, 0x01, (unsigned char*)&value, sizeof(value), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Setting Homing Speed +ve Completed\n");
  value = 50000;
  if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6099, 0x02, (unsigned char*)&value, sizeof(value), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  //printf("Setting Homing Speed -ve Completed\n");
  usleep(500000);
  //Jog Now


  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, data_size, &abortCode);

  while(1){
    if((errorCode = ecrt_master_sdo_upload(master0, 0, STATUS_WORD, (unsigned char *)&statusWord, sizeof(statusWord), &resultSize, &abortCode)) < 0) {
      //printf("Abort Message : %x\n", abortCode);
      return;
    }
    //printf("Status Word Value --> %d \n",statusWord);
    //printf("Status Word Value in hex --> %x \n",statusWord);
    resp = decimalToBinary(statusWord);
    /*//printf("Status Word in Binary --> ");
    for(k=0; k<64; k++){
      //printf("%d",resp[k]);
    }*/
    //printf("\n");

    if(statusWord == 5687){
      //printf("Homing Position reached. Will set offset for 0th index\n");
      cw = 0x000F;
      if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&cw, sizeof(cw), &abortCode)) < 0) {
        //printf("Abort Message : %x\n", abortCode);
        return;
      }
      cw = 0x004F;
      if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&cw, sizeof(cw), &abortCode)) < 0) {
        //printf("Abort Message : %x\n", abortCode);
        return;
      }
      //printf("Set PP Mode\n");
      cw_value = 0x1;
      if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6060, 0x00, (unsigned char*)&cw_value, sizeof(cw_value), &abortCode)) < 0) {
        //printf("Abort Message : %x\n", abortCode);
        return;
      }
      zmq_send(requester, "HOMING", 6, 0);
      //printf("Set Offset Value --\n");
      value = atoi(position);
      //printf("Homing Offset value is --> %d\n",value);
      errorCode = ecrt_master_sdo_download(master0, 0, TARGET_POSITION, (unsigned char *)&value, sizeof(value), &abortCode);
      //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
      //printf("errorCode %d\n",errorCode);
      usleep(10000);
      Jog(0,1,0);
      //printf("Jog Completed\n");

      return;
    }
    usleep(100000);
  }
}

void setPPMode(){
	uint8_t cw_value = 0x1;
	uint32_t abortCode = 0;
	master0 = drive_master[0];
	if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6060, 0x00, (unsigned char*)&cw_value, sizeof(cw_value), &abortCode)) < 0) {
	  //printf("Abort Message : %x\n", abortCode);
	  return;
	}
}

void setPVMode(){
	uint8_t cw_value = 0x3;
	uint32_t abortCode = 0;
	master0 = drive_master[0];
	if((errorCode = ecrt_master_sdo_download(master0, 0, 0x6060, 0x00, (unsigned char*)&cw_value, sizeof(cw_value), &abortCode)) < 0) {
	  //printf("Abort Message : %x\n", abortCode);
	  return;
	}
}

void resetAlarms(){
  //printf("Will reset alarms\n");
  printf("Inside reset Alarms\n");
  printf("Registry read completed\n");
  EMG_ACTIVE = 1;
  UNDER_MOTION = 0;
  printf("Check emergency value before reset -> %d\n",EMG_ACTIVE);
  CLAMP_ERROR = 0;
  DECLAMP_ERROR = 0;
  clearBuffer();
  faultReset(1);
  clearAllOutput(1);
  //Reset to Profile Position mode
  setPPMode();
}

void manual_jog(void *args){
  //JOG_RPM
  int i=0;
  int sum = 0;
  int x = 0;
  int *registry;
  uint32_t val;
  uint32_t abortCode = 0;
  int counter = 0;
  //uint8_t mode_val = 3; //3 for profile velocity mode
  int errorCode = 0;
  unsigned short controlWord;
  unsigned short statusWord;
  uint8_t cw_value = 0x3;
  size_t data_size = sizeof(cw_value);
  unsigned long maxFlowingError = 1000000;
  position_params *pp = args;

  master0 = drive_master[pp->drive_id];
  sum = atoi(pp->position);

  //printf("Drive Id is --> %d, Position is --> %s\n",pp->drive_id, pp->position);
  printf("Sum status value is --> %d\n", sum);
  if(sum == 1 || sum ==2){
    //+ve or -ve movement
    powerOn(1);
    printf("Power on complete\n");
    if(CL_DL == 1){
      printf("Clamp Declamp is enabled..\n");
      // counter = getTimingCounter();
	  counter = COUNTER;
      while(1){
        counter--;
        if(counter < 0){
          DECLAMP_ERROR = 1;
          return;
        }
        registry = readInputSignal(1);
        printf("%d",registry[21]);
        if(registry[21] == 1){
          printf("Got De-Clamp Signal. Now next step\n");
          break;
        }
        usleep(1000);
      }
    }

  }
  //printf("Action value --> %d\n",sum);
  //Set all parameters
  //sudo ethercat download 0x6081 0 2000000 - PROFILE_VELOCITY 60FF
  data_size = sizeof(maxFlowingError);

  //sudo ethercat download 0x6083 0 5000000 - PROFILE_ACCELERATION
  maxFlowingError = 100000000;
  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, PROFILE_ACCELERATION, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message Profile Acceleration: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  //sudo ethercat download 0x6084 0 2500000 - PROFILE_DECELERATION
  //maxFlowingError = 500000;
  maxFlowingError = 100000000;
  data_size = sizeof(maxFlowingError);
  errorCode = ecrt_master_sdo_download(master0, 0, PROFILE_DECELERATION, (unsigned char *)&maxFlowingError, data_size, &abortCode);
  //printf("Abort Message Profile Deceleration: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);

  if(sum == 1){
    //printf("JOG IN +ve\n");
    maxFlowingError = JOG_RPM;
    errorCode = ecrt_master_sdo_download(master0, 0, 0x60FF,0, (unsigned char *)&maxFlowingError, data_size, &abortCode);
    //printf("Abort Message Setting RPM: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);
    data_size = sizeof(cw_value);
    //Mode in positive direction
    errorCode = ecrt_master_sdo_download(master0, 0, OPERATION_MODE, (unsigned char*)&cw_value, data_size, &abortCode);
    //printf("Abort Message Setting Mode: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);
  }
  else if(sum == 2){
    //printf("JOG IN -ve\n");
    maxFlowingError = -1*JOG_RPM;
    data_size = sizeof(maxFlowingError);
    errorCode = ecrt_master_sdo_download(master0, 0, 0x60FF,0, (unsigned char *)&maxFlowingError, data_size, &abortCode);
    //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);
    //Move in negative direction
    data_size = sizeof(cw_value);
    errorCode = ecrt_master_sdo_download(master0, 0, OPERATION_MODE, (unsigned char*)&cw_value, data_size, &abortCode);
    //printf("Abort Message OPERATION MODE: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);

  }else if(sum == 3){
    //Stop Jogging
    maxFlowingError = 0;
    printf("Stopping JOG\n");
    errorCode = ecrt_master_sdo_download(master0, 0, 0x60FF,0, (unsigned char *)&maxFlowingError, data_size, &abortCode);
    printf("60FF value set is complete\n");
    if(CL_DL == 1){
      printf("CL DL is enabled. Stop now\n");
      powerOff(1);
      printf("Power off compelete\n");
      printf("Waiting for Clamp Signal\n");
      // counter = getTimingCounter();
	  counter = COUNTER;
      while(1){
        counter--;
        if(counter < 0){
          CLAMP_ERROR = 1;
          return;
        }
        registry = readInputSignal(1);
        //  //printf("%d",registry[20]);
        if(registry[20] == 1){
          printf("Got Clamp Signal. Now send finish signal\n");
          break;
        }
      }
    }
    //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
    //printf("errorCode %d\n",errorCode);
    /*cw_value = 0x1;
    errorCode = ecrt_master_sdo_download(master0, 0, OPERATION_MODE, (unsigned char*)&cw_value, sizeof(cw_value), &abortCode);
    controlWord = 0x0007;
    data_size = sizeof(controlWord);
    errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, sizeof(controlWord), &abortCode);
    controlWord = 0x004f;
    data_size = sizeof(controlWord);
    errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char*)&controlWord, sizeof(controlWord), &abortCode);*/
    printf("\n\n*********Set Position Complete************\n\n\n");
  }
}

void clearBuffer(void *args){
  /*printf("Clearing Multiturn Data\n");
  uint16_t value = 0x7;
  size_t data_size = sizeof(value);
  uint32_t abortCode = 0;
  uint32_t val_1 = 0x200;
  //uint16_t abortCode = 0;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040,0x0, (unsigned char *)&value, data_size, &abortCode);
  value = 0x31;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D01,0x0, (unsigned char *)&value, data_size, &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D00,0x1, (unsigned char *)&val_1, data_size, &abortCode);
  val_1 = 0x0;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D00,0x1, (unsigned char *)&val_1, data_size, &abortCode);

  printf("Multi Turn Data Cleared\n");*/
  printf("Buffer not cleared\n");
}

void resetMultiTurnData(void *args){
  printf("Clearing Multiturn Data\n");
  uint16_t value = 0x7;
  size_t data_size = sizeof(value);
  uint32_t abortCode = 0;
  uint32_t val_1 = 0x200;
  //uint16_t abortCode = 0;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040,0x0, (unsigned char *)&value, data_size, &abortCode);
  value = 0x31;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D01,0x0, (unsigned char *)&value, sizeof(value), &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D00,0x1, (unsigned char *)&val_1, sizeof(val_1), &abortCode);
  val_1 = 0x0;
  errorCode = ecrt_master_sdo_download(master0, 0, 0x4D00,0x1, (unsigned char *)&val_1, sizeof(val_1), &abortCode);

  printf("Multi Turn Data Cleared\n");

}


void receiverQueue(){
  //Single receiver. Which will take action based on the received message.
  //char buffer [20];
  char resp[100];
  char resp1[100];
  char *x_status;
  char *y_status;
  int drive_id;
  int action;
  int curr_pos;
  char *value;
  char *tmp;
  int err;
  int val;
  int32_t lim;
  pthread_t local_tid, halt_tid;
  position_params *pos_param = malloc(sizeof(position_params));
  int n;
  char buffer[BUFSIZE];
  int sock_fd = getConnection();  /* Use the function here */
  uint32_t abortCode = 0;
  size_t data_size;
  unsigned long maxFlowingError;
  //printf("Completed Socket connection\n");


  while(1){
    //zmq_recv(requester, buffer, 20, 0);
    bzero(buffer, BUFSIZE);
    local_tid = NULL;
    n = read(sock_fd, buffer, BUFSIZE);
    printf("Command received --> %s\n",buffer);
    if (n < 0)
      error("ERROR reading from socket");
    //printf("Message from server: %s", buffer);
    //printf("MSG-1\n");
    tmp = strtok(buffer, " "); //first_part points to "drive id"
    drive_id = atoi(tmp);
    tmp = strtok(NULL, " ");   //sec_part points to "action"
    action = atoi(tmp);
    position = strtok(NULL," ");   //third_part points to "value"

    //printf("Position value extracted from the buffer is %s\n",position );

    strcpy ( pos_param->position, position) ;
    id = drive_id - 1;
    pos_param->drive_id = id;
    pos_param->mode = 1;
    //printf("Drive ID %d, Action %d, Value %s--\n",drive_id,action,position);
    //printf("Will execute respective commands\n");
    step_mode = 0;
    if(action == 2 || action == 31){
      printf("Will move in positive direction\n");
      printf("Object Init completed\n");
      if(action == 31){
        ZERO_REF = 1;
        pos_param->mode = 0;
      }else{
        pos_param->mode = 1;
      }

      //err = pthread_create(&local_tid, NULL, &moveToPosition, NULL);
      err = pthread_create(&local_tid, NULL, &moveToPosition, pos_param);
      pthread_detach(local_tid);
    }
    else if (action == 3 || action == 32){
      //printf("Will move in negative direction\n");
      //printf("Object init completed\n");
      local_tid = NULL;
      if(action == 32){
        ZERO_REF = 1;
        pos_param->mode = 0;
      }else{
        pos_param->mode = 1;
      }
      err = pthread_create(&local_tid, NULL, &moveToNegativePosition, pos_param);
      pthread_detach(local_tid);
      //printf("Thread creation complete\n");
      if(err){
        //printf("Unable to get status. Please restart");
        zmq_send (requester, "RESTART", 7, 0);
      }

      //pthread_join(local_tid, NULL);
    }
    else if (action == 4){
      //printf("Will perform Homing operation\n");
      startHoming(drive_id-1);
    }
    else if(action == 5){
      //ECS settings.
      printf("Will not modify ECS settings\n");
      //ECS_ENABLED = atoi(position);
      //printf("%d\n",ECS_ENABLED);
    }
    else if (action == 6){
      //Send Finish Signal
      //printf("Will send finish signal now\n");
    }
    else if (action == 7){
      //printf("Power Off drive");
      powerOff(1);
    }
    else if (action == 8){
      //printf("Power On drive");
      STOP_PROGRAM = 0; //Reset stop program..
      powerOn(1);
    }
    else if (action == 9){
      //printf("Running in step mode\n");
      //printf("Position is %s\n",position);
      pos_param->mode = 2; //Set Mode == 2 for Step..
      err = pthread_create(&local_tid, NULL, &moveToPosition, pos_param);
      pthread_detach(local_tid);
      zmq_send(requester, "TARGET", 6, 0);
      zmq_send(requester, "TARGEE", 6, 0);
      printf("Completed Sending ZMQ");
      //pthread_join(local_tid, NULL);
    }
    else if (action == 10){
      //printf("Halting the movement\n");
      powerOff(1);
      //err = pthread_create(&local_tid, NULL, &halt, pos_param);
    }
    else if (action == 11){
      //printf("Jog in positive direction\n");
      powerOn(1);
      pos_param->mode = 0;
      err = pthread_create(&local_tid, NULL, &moveToPosition, pos_param);
      pthread_detach(local_tid);
      zmq_send(requester, "TARGET", 6, 0);
      zmq_send(requester, "TARGEE", 6, 0);
      printf("Completed Sending ZMQ");
    }

    else if (action == 12){
      //printf("Jog in negative direction\n");
      powerOn(1);
      pos_param->mode = 0;
      err = pthread_create(&local_tid, NULL, &moveToNegativePosition, pos_param);
      pthread_detach(local_tid);
      zmq_send(requester, "TARGET", 6, 0);
      zmq_send(requester, "TARGEE", 6, 0);
      printf("Completed Sending ZMQ");
    }
    else if (action == 13){
      //printf("Reset all alarms\n");
      //err = pthread_create(&local_tid, NULL, &moveToNegativePosition, pos_param);
      resetAlarms();
    }
    else if (action == 14)
    {
      //printf("Toggle Emergency\n");
      if(atoi(position) == 1){
        //printf("Send Emergency Alarm\n");
        toggleEmergency(id, 1);
      }
    }
    else if(action == 15)
    {
      val=atoi(position);
      maxFlowingError = val;
      data_size = sizeof(maxFlowingError);
      //printf("Command received. RPM to be changed now. RPM value is %d\n",val);
      errorCode = ecrt_master_sdo_download(master0, 0, 0x6081,0, (unsigned char *)&maxFlowingError, sizeof(maxFlowingError), &abortCode);
      //printf("Abort Message upon changing RPM: %x\n", abortCode);
      //printf("errorCode %d\n",errorCode);
    }
    else if(action == 16){
      //printf("Will start jog in velocity mode\n");
      if(atoi(position) == 1 || atoi(position) == 3){
        //printf("Run in Positive direction\n");
        pos_param->mode=0;
        err = pthread_create(&local_tid, NULL, &manual_jog, pos_param);
        pthread_detach(local_tid);
      }else if(atoi(position) == 2){
        //printf("Run in Reverse direction\n");
        pos_param->mode=0;
        err = pthread_create(&local_tid, NULL, &manual_jog, pos_param);
        pthread_detach(local_tid);
      }
    }
    else if(action == 17){
      //Setting Feed rate for Jog in manual mode
      //printf("Setting RPM for JOG\n");
      if(atoi(position) > 0){
        JOG_RPM = atoi(position);
      }else{
        //printf("WARNING -- Invalid position data\n");
      }
      //printf("JOG RPM value is --> %d\n",JOG_RPM);
    }
    else if(action == 18){
      //Configure in PP Mode
      //printf("Configuring the drive in PP Mode\n");
      Configure(0);
      //printf("Configuration Completed\n");
    }
    else if(action == 19){
      ecssignal = 1;
    }
    else if(action == 20){ //ENABLE ECS from Settings
      printf("Enable ECS\n");
      ECS_ENABLED = 1;
    }
    else if(action == 21){ //DISABLE ECS from settings
      printf("Disable ECS\n");
      ECS_ENABLED = 0;
    }
    else if(action == 22){ //Homing +ve from Settings
      printf("Homing +ve\n");
      HOMING_DIR = 1;
    }
    else if(action == 23){ //Homing -ve from settings
      printf("Homing -ve\n");
      HOMING_DIR = 0;
    }
    else if(action == 24){ //Drive Direction +ve
      printf("Drive +ve\n");
      DRIVE_DIR = 1;
    }
    else if(action == 25){ //Drive Direction -ve
      printf("Drive -ve\n");
      DRIVE_DIR = 0;
    }
    else if(action == 26){ //Drive Direction +ve
      printf("Clamp Declamp +ve\n");
      CL_DL = 1;
    }
    else if(action == 27){ //Drive Direction -ve
      printf("Clamp Declamp -ve\n");
      CL_DL = 0;
    }
    else if(action == 28){ //Finish Signal
      printf("Enable Finish Signal\n");
      FIN_SIGNAL = 1;
    }
    else if(action == 29){ //Drive Direction -ve
      printf("=========================\n");
      printf("DISABLE FIN SIGNAL\n");
      printf("=========================\n");
      FIN_SIGNAL = 0;
      printf("%d\n",FIN_SIGNAL);
      printf("=========================\n");
    }//31 & 32 is used for Homing
    else if(action == 33){
      printf("Received Stop program..\n");
      STOP_PROGRAM = 1;
      printf("Stop Complete\n");
    }
    else if(action == 34){
      printf("Received Clear Multi Turn Data..\n");
      err = pthread_create(&local_tid, NULL, &resetMultiTurnData, NULL);
      pthread_detach(local_tid);
      printf("Stop Complete\n");
    }
    else if(action == 35){
      printf("\n\nReceived timing signal\n");
      TIMING = atoi(position)*1000;
    }
    else if(action == 36){
      printf("\n\nReceived POT Limit\n");
      lim = atoi(position);
      printf("%d\n",lim);
      errorCode = ecrt_master_sdo_download(master0, 0, 0x607B,0x01, (unsigned char *)&lim, sizeof(lim), &abortCode);
    }
    else if(action == 37){
      printf("\n\nReceived NOT Limit\n");
      lim = atoi(position);
      printf("%d\n",lim);
      errorCode = ecrt_master_sdo_download(master0, 0, 0x607B,0x02, (unsigned char *)&lim, sizeof(lim), &abortCode);
    }
    else if(action == 38){
      printf("\n\nReceived Power Off\n");
      powerOff(1);
    }
	else if(action == 39){
      printf("\n\nReceived CLDL Timing delay\n");
      // powerOff(1);
	  COUNTER = atoi(position);
    }
    else{
      //printf("Disable Emergency\n");
      toggleEmergency(id, 0);
    }
      //err = pthread_create(&local_tid, NULL, &moveToNegativePosition, pos_param);
    }
  }
void incrementalJog(int dir){
  int i=0;
  int sum = 0;
  int x = 0;
  uint32_t val;
  uint32_t abortCode = 0;


  val = 2500;
  //printf("----%d-----\n",val);

  if(dir==1){
      //printf("Rotate in positive direction");
  }else{
      //printf("Rotate in negative direction");
      val = val*-1;
  }

  errorCode = ecrt_master_sdo_download(master, 0, TARGET_POSITION, (unsigned char *)&val, sizeof(val), &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);
  Jog(0,1,1);
}


void pollUserCommands(){
    char *resp, *first_part, *sec_part;
    char *CMD_JOG = "JOG";
    char *EXECUTE_PROGRAM = "START_PROGRAM";
    char buff[1024];
    int read_size;
    int val;
    //printf("Started polling user commands\n");
    /*while(1){

        recv(clientSocketSender, recv_buffer, 1024, 0);
        //printf("Received buffer %s", recv_buffer);
    }*/
    while(1){
        while((read_size = readData(buff)) > 0 )
        {
            ////printf("Read Size1 --> %d, Data1 --> %s", read_size, buff);
            first_part = strtok(buff, " "); //first_part points to "user"
	          sec_part = strtok(NULL, " ");   //sec_part points to "name"
            //printf("RECEIVED KEY -->%s, VALUE -->%s, comparison status -->%d,\n", first_part, sec_part, strcmp(first_part,CMD_JOG));
            if(strcmp(first_part,CMD_JOG) == 0){
                //printf("Its a JOG.\n");
                val = atoi(sec_part);
                //printf("Direction value is --> %d--\n",val);
                incrementalJog(val);
            }
            else if(strcmp(first_part,EXECUTE_PROGRAM) == 0){
                //printf("Start a New Program.\n");
                START_EXECUTION = 1;
            }

	      }

        if(read_size == 0)
        {
            puts("Client disconnected");
            fflush(stdout);
        }
        else if(read_size == -1)
        {
            perror("recv failed");
        }
    }
}

void SetupSlave()
{
  unsigned short controlWord;
  unsigned short statusWord;
  int errorCode = 0;
  uint32_t abortCode = 0;
  unsigned char period = 10;
  //unsigned char operationMode = 8; #TODO
  unsigned char operationMode = 1;
  unsigned long maxFlowingError = 1000000;
  if((errorCode = ecrt_master_sdo_download(master, 0, 0x6065, 0x00, (unsigned char*)&maxFlowingError, 4, &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  if((errorCode = ecrt_master_sdo_download(master, 0, OPERATION_MODE,(unsigned char *)&operationMode, 1, &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  if((errorCode = ecrt_master_sdo_download(master, 0, 0x60C2, 0x01, &period, 1, &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
    return;
  }
  while((statusWord = GetStatus(0)) != OPERATION_ENABLED) {
    if(statusWord == FAULT) {
      controlWord = FAULT_RESET;
    }
    else if(statusWord == SWITCH_ON_DISABLED) {
      controlWord = SHUTDOWN;
    }
    else if(statusWord == READY_TO_SWITCH_ON) {
      controlWord = SWITCH_ON;
    }
    else if(statusWord == SWITCH_ON_ENABLED) {
      controlWord = ENABLE_OPERATION;
    }

    if((errorCode = ecrt_master_sdo_download(master, 0, CONTROL_WORD,(unsigned char *)&controlWord, 2, &abortCode)) < 0) {
      //printf("Abort Message : %x\n", abortCode);
      return;
    }
    usleep(1000);
  }
}
/*************************************************************************

***/
void clean_data(char *line_feed){
  //char str[]= "ls -l";
  char *  p    = strtok (line_feed, " ");
  int i;
  int n_spaces = 0;


  /* split string and append tokens to 'res' */

  while (p) {
    res = realloc (res, sizeof (char*) * ++n_spaces);

    if (res == NULL)
      exit (-1); /* memory allocation failed */

    res[n_spaces-1] = p;

    p = strtok (NULL, " ");
  }
  count = n_spaces;
  /* realloc one extra element for the last NULL */

  res = realloc (res, sizeof (char*) * (n_spaces));
  res[n_spaces] = 0;

  for(i=0;i <count; i++){
    ////printf("res[%d][%lu] = %c\n",i, strlen(res[i])-1, res[i][strlen(res[i])-1]);
    if(res[i][strlen(res[i])-1] == ';'){
      res[i][strlen(res[i])-1] = 0;
    }
    ////printf("res[%d] = %s\n",i, res[i]);
  }

  return;
  //return res;
  /* free the memory allocated */
  //free (res);
}

void setMaxProfileSpeed(){
  unsigned long value = MAX_TARGET_VELOCITY;
  uint32_t abortCode = 0;
  //uint32_t val = 5000000; // 5000000/90 rpm
  int errorCode;
  if((errorCode = ecrt_master_sdo_download(master, 0, TARGET_VELOCITY, (unsigned char*)&value, sizeof(value), &abortCode)) < 0) {
    //printf("Abort Message : %x\n", abortCode);
  }
}

void setFeedRate(char *feed_string){
  int len = strlen( feed_string );
  int sum = 0;
  int k = 0;
  int i = 0;
  int x = 0;
  for (i = len-1; i > 0; i++){
    int x = feed_string[i] - '0';
    sum = sum + x*(pow(10,(len-1)-i));
  }
  //printf("Sum --> %d",sum);
}

void moveNow(int position){
  int i=0;
  int sum = 0;
  int x = 0;
  uint32_t val;
  uint32_t abortCode = 0;
  int errorCode = 0;

  val = position;

  errorCode = ecrt_master_sdo_download(master, 0, TARGET_POSITION, (unsigned char *)&val, sizeof(val), &abortCode);
  //printf("Abort Message TARGET_POSITION: %x\n", abortCode);
  //printf("errorCode %d\n",errorCode);
  Jog(0,1,0);
}


int string_exists(char *actual_string, char *substring){
  char *ret = strstr(actual_string,substring);
  ////printf("Actual String --> %s, Substring --> %s\n",actual_string, substring );
  ////printf("Return value --> %s\n", ret);
  ////printf("length of string --> %d\n",strlen(ret));
  if(ret == NULL){
    return -1;
  }
  return 0;
}
void waitForExternalECS(){
  //uint32_t val=0xFFFF;
  //unsigned short v = 0xFFFF;
  unsigned long int v = 0xFFFFFFFF;
  uint32_t abortCode = 0;
  int errorCode = 0;
  size_t resultSize = 0;
  int k;
  int *resp;
  while(1){
    if(ecrt_master_sdo_upload(master, 0, INPUT_STATUS, (unsigned char *)&v, sizeof(v), &resultSize, &abortCode) <0 ){
      //printf("\n\n\n-----ALERT:: FAILURE------\n\n\n");
    }
    ////printf("60FD Registry Value --> %lu\n",v);
    resp = longToBinary(v);
    usleep(10);
    /*for(k=0; k<64; k++){
      //printf("%d",resp[k]);
    }*/
    //printf("\n");
    ////printf("18th -> %d, 19th -> %d, 20th -> %d\n",resp[18],resp[19],resp[20]);
    if(resp[19] == 1){
        //printf("I gotta signal. Time to move..");
        for(k=0; k<64; k++){
          //printf("%d",resp[k]);
        }
        return;
    }
  }
}

void pollAlarms(){
  int *registry;
  char str[20];
  char str1[20];
  uint32_t abortCode = 0;
  int errorCode = 0;
  master0 = drive_master[id];
  unsigned short value = 0xFFFF;
  size_t resultSize=0;
  printf("Inside poll Alarm function\n");
  while(1){
    registry = readInputSignal(1);
	if(registry[19] == 1 && UNDER_MOTION ==1){
		registry[19] = 0;
	}
    sprintf(str, "ALARM %d%d%d%d%d%d%d%d", registry[0],registry[1],registry[2],registry[19],registry[20],registry[21],registry[22],registry[23]);
    zmq_send (requester, str, 13, 0);

    registry = readOutputSignal(1);
    sprintf(str,"OUTPUT %d%d", registry[0],registry[16]);
    zmq_send (requester, str, 9, 0);
    ////printf("Sending alarm status %s\n",str);
    registry = readAlarms(1);
    /*sprintf(str,"FAULT %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d", registry[0],registry[1],registry[2],registry[3],registry[4],registry[5],registry[6],registry[7],registry[8],
                registry[9],registry[10],registry[11],registry[12],registry[13],registry[14],registry[15]);*/
    zmq_send (requester, str,22, 0);
    if(CLAMP_ERROR == 1){
      printf("Clamp Error\n");
      sprintf(str1, "ERROR 65378");
      zmq_send(requester, str1, 11,0);
    }else if(DECLAMP_ERROR == 1){
      printf("De-Clamp Error\n");
      sprintf(str1, "ERROR 65379");
      zmq_send(requester, str1, 11,0);
    }else{
      errorCode = ecrt_master_sdo_upload(master0, 0, 0x603F,0x00, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
	  if(errorCode < 0){
		  printf("Failed to execute command..");
		  exit(-1);
	  }
      sprintf(str1, "ERROR %u",value);
      zmq_send(requester, str1, 11,0);
    }

    usleep(100000); //TODO
	memset(registry, 0, sizeof(registry));
    //usleep(3000000);
  //read the Alarm status
  }
}


void faultReset(int id){
  master0 = drive_master[0];
  int errorCode = 0, n=0;
  uint32_t abortCode = 0;
  int *binary_value;
  //uint16_t value = 0x0007;
  unsigned short value = 0xFFFF;
  size_t resultSize=0;

  //Get all the params..

  ecrt_master_sdo_upload(master0, 0, 0x6040,0x00, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
  binary_value = longToBinary(value);
  binary_value[7] = 1;
  //printf("\n");
  value = binaryToLongValue(binary_value);
  //printf("Converted decimal value --> %lu\n",value);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x6040, 0x00, (unsigned char *)&value, sizeof(value), &abortCode);
  usleep(150000);

  //printf("Fault reset Complete\n");
}

void clearAllOutput(int id){
  master0 = drive_master[0];
  int errorCode = 0, n=0, i=0;
  uint32_t abortCode = 0;


  int *binary_value;
  //uint16_t value = 0x0016;
  unsigned long int value = 0xFFFFFFFF;
  size_t resultSize;

  //Get all the params..

  ecrt_master_sdo_upload(master0, 0, 0x60FE,0x01, (unsigned char *)&value, sizeof(value), &resultSize, &abortCode);
  binary_value = longToBinary(value);

	binary_value[0]=0;
  binary_value[16]=0;

  value = binaryToLongValue(binary_value);
  //printf("Converted decimal value --> %lu\n",value);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x02, (unsigned char *)&value, sizeof(value), &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x01, (unsigned char *)&value, sizeof(value), &abortCode);
  value = binaryToLongValue(binary_value);
  usleep(500000);
  //errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x02, (unsigned char *)&value, sizeof(value), &abortCode);
  errorCode = ecrt_master_sdo_download(master0, 0, 0x60FE, 0x01, (unsigned char *)&value, sizeof(value), &abortCode);
  //printf("Sending Interrupt Signal\n");
}


int main(int argc, char **argv)
{
//ec_slave_config_t *sc;
  struct sigaction sa;
  struct itimerval tv;
  int err;
  int cnt;
  int drive_index = 0;
  char buffer [10];
  char str[50];
  //printf("Connect to ZeroMQ now\n");
  void *context = zmq_ctx_new ();
  requester = zmq_socket (context, ZMQ_PAIR);
  zmq_connect (requester, "tcp://localhost:5556");
  //printf("Connection to zmq complete\n");
  //printf("Send dummy message\n");
  zmq_send (requester, "HELL0", 5, 0);
  usleep(100000);
  buffer[0] = '\0';
  //char pos[] = "2000000";
  master0 = ecrt_request_master(0);
  if(!master0){
    //printf("X Axis is not active..\n");
    zmq_send (requester, "FAIL X", 6, 0);
  }else{
    //printf("ACTIVE AXIS -> X\n");
    drive_master[ACTIVE_DRIVE] = master0;
    ACTIVE_DRIVE++;
  }

  master1 = ecrt_request_master(1);
  if(!master1){
    //printf("Y Axis is not active..\n");
    zmq_send (requester, "FAIL Y", 6, 0);
  }else{
    //printf("ACTIVE AXIS -> Y\n");
    drive_master[ACTIVE_DRIVE] = master1;
    ACTIVE_DRIVE++;
  }

  //printf("Object Initialization complete, total drives --> %d\n", ACTIVE_DRIVE);
  sprintf(str, "DRIVE_COUNT %d", ACTIVE_DRIVE);
  zmq_send(requester, str, 13, 0);
  for(cnt=0; cnt<ACTIVE_DRIVE; cnt++){
    //printf("Inside the activation loop\n");
    domain0 = ecrt_master_create_domain(drive_master[cnt]);
    //printf("Creating master domain completed\n");
    if(!domain0)
    {
      //printf("Invalid Domain %d. Hence Quitting..\n",cnt);
      sprintf(str, "FAIL %d", cnt);
      zmq_send (requester, str, 6, 0);
      return -1;

    }
    //printf("Domain is valid..\n");
    if(!(sc_epos3 = ecrt_master_slave_config(drive_master[cnt], SLAVE_DRIVE_0, MAXON_EPOS3))){
      fprintf(stderr, "Failed to get slave configuration for drive %d. \n", drive_master[cnt]);
      sprintf(str, "FAIL %d", cnt);
      zmq_send (requester, str, 6, 0);
      return -1;
    }

    //Now Set 0x608F -> 20000, 0x6091 -> 2 & 0x6092 -> 2


    //printf("Master Config is completed. Configuring Slave %d now", cnt);
    Configure(cnt);
    //Configure();
    //printf("Configure Completed for drive -> %d\n", cnt);
    usleep(500000);
    //printf("Connected to interface server\n");
    //printf("Powering On\n");
    PowerOn(cnt);
    //printf("Power On Complete\n");
    usleep(500000);
    //Now create a thread to send/receive messages to/from server
    //err = pthread_create(&(tid[cnt]), NULL, &pollStatus, cnt);

    //printf("Will continuously poll now on drive --> %d..\n", cnt);
  }

  //printf("\n\n===Main Thread ID===========%d===============\n\n",getpid());
  //printf("Creating thread\n");
  err = pthread_create(&(tid[0]), NULL, &pollStatus, NULL);
  err = pthread_create(&(tid[1]), NULL, &receiverQueue, NULL);
  err = pthread_create(&(tid[2]), NULL, &pollAlarms, NULL);
  //err = pthread_create(&(tid[0]), NULL, &test1, NULL);
  //err = pthread_create(&(tid[1]), NULL, &test2, NULL);
  // pthread_detach(tid[0], NULL);
  // pthread_detach(tid[1], NULL);
  // pthread_detach(tid[2], NULL);

  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_join(tid[2], NULL);

  /*if(err){
    //printf("Unable to get status. Please restart");
    zmq_send (requester, "RESTART", 7, 0);
  }*/

}

