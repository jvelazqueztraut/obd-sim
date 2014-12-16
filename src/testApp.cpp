#include "testApp.h"
#include "windowssimport.h"
#include <Winsock2.h>

#define OBDSIM_DEVICE_IDENTIFIER_STRING "ELM327_SIMULATOR"
#define OBDSIM_ELM_DEVICE_STRING "OBDSIM_DEVICE"
#define OBDSIM_ELM_VERSION_STRING "OBDSIM_VERSION"

char *line; // Single line from the other end of the device
char previousline[1024] = "GARBAGE"; // Blank lines mean re-run previous command

const char *newline_cr = "\r";
const char *newline_crlf = "\r\n";

unsigned char speed = 0;
unsigned char rpm = 0;
unsigned char temp = 0;
unsigned char nafta = 0;
unsigned char runtime = 0;

/// Do an elm reset [ATZ or similar]
void obdsim_elmreset(struct simsettings *s) {
	s->e_headers = ELM_HEADERS;
	s->e_spaces = ELM_SPACES;
	s->e_echo = ELM_ECHO;
	s->e_linefeed = ELM_LINEFEED;
	s->e_timeout = ELM_TIMEOUT;
	s->e_adaptive = ELM_ADAPTIVETIMING;
	s->e_dlc = ELM_DISPLAYDLC;
}

int set_obdprotocol(const char *prot, struct simsettings *ss) {
	if(NULL == prot) {
		return -1;
	}

	// Allow spaces in various places even though behaviour isn't very well defined
	const char *p = prot;
	while(' ' == *p && '\0' != *p) p++;
	if('\0' == *p) {
		return -1;
	}

	int e_autoprotocol = 0;
	if('A' == *p || 'a' == *p) {
		e_autoprotocol = 1;
		p++;
	}
	while(' ' == *p && '\0' != *p) p++;

	struct obdiiprotocol *e_protocol = find_obdprotocol(p);
	p++;
	if(NULL == e_protocol) {
		return -1;
	}
	while(' ' == *p && '\0' != *p) p++;

	if('A' == *p || 'a' == *p) {
		e_autoprotocol = 1;
	}

	ss->e_protocol = e_protocol;
	ss->e_autoprotocol = e_autoprotocol;

	return 0;
}

struct obdiiprotocol *find_obdprotocol(const char *protocol_num) {
	int i;
	for(i=0; i<sizeof(obdprotocols)/sizeof(obdprotocols[0]); i++) {
		if(*protocol_num == obdprotocols[i].protocol_num) {
			return &(obdprotocols[i]);
		}
	}
	return NULL;
}

/// Initialise the variables in an ECU
void obdsim_initialiseecu(struct obdgen_ecu *e) {
	//e->simgen = NULL;
	e->ecu_num = 0;
	e->seed = NULL;
	e->lasterrorcount = 0;
	e->ffcount = 0;
	e->dg = 0;
	e->customdelay = 0;
	memset(e->ff, 0, sizeof(e->ff));
}

/// Initialse all variables in a simsettings
void obdsim_initialisesimsettings(struct simsettings *s) {
	s->e_autoprotocol = 1;
	set_obdprotocol(OBDSIM_DEFAULT_PROTOCOLNUM, s);

	s->benchmark = OBDSIM_BENCHMARKTIME;
	s->e_currentvoltage = OBDSIM_BATTERYV;

	s->device_identifier = strdup(OBDSIM_DEVICE_IDENTIFIER_STRING);
	s->elm_device = strdup(OBDSIM_ELM_DEVICE_STRING);
	s->elm_version = strdup(OBDSIM_ELM_VERSION_STRING);

	s->ecu_count = 0;
	int i;
	for(i = 0; i<OBDSIM_MAXECUS; i++) {
		obdsim_initialiseecu(&s->ecus[i]);
		s->ecudelays[i].ecu = NULL;
		s->ecudelays[i].delay = 0;
	}

	obdsim_elmreset(s);
}

//--------------------------------------------------------------
void testApp::setup(){
	ofSetFrameRate(30);

	ss = new struct simsettings;
	obdsim_initialisesimsettings(ss);

	string winport="COM27";
	sp = new WindowsSimPort(winport.c_str());
	
	if(NULL == sp || !sp->isUsable()) {
		fprintf(stderr,"Error creating virtual port\n");
		return;
	}
	char *slave_name = sp->getPort();
	if(NULL == slave_name) {
		printf("Couldn't get slave name for pty\n");
		delete sp;
		return;
	}
	printf("SimPort name: %s\n", slave_name);

	printf("Successfully initialised obdsim, entering main loop\n");

	sp->setEcho(ss->e_echo);

	frame=0;
}

//--------------------------------------------------------------
void testApp::update(){
	// Now the actual choise-response thing
	line = sp->readLine(); // This is the input line

	if(NULL != line){
		for(int i=strlen(line)-1;i>=0;i--) { // Strlen is expensive, kids.
			line[i] = toupper(line[i]);
		}

		printf("%s ", line);

		if(NULL != strstr(line, "EXIT")) {
			printf("Received EXIT via serial port. Sim Exiting\n");
			ofExit();
			return;
		}

		string response;

		if('A' == line[0] && 'T' == line[1]) {
			// If we recognised the command
			int command_recognised = 0;
			command_recognised = parse_ATcmd(ss,sp,line,response);
			if(0 == command_recognised) {
				response = ELM_QUERY_PROMPT;
			}
		}
		if('0' == line[0] && '1' == line[1]) {
			// If we recognised the command
			int command_recognised = 0;
			command_recognised = parse_01cmd(ss,sp,line,response);
			if(0 == command_recognised) {
				response = ELM_QUERY_PROMPT;
			}
		}
		else{
			printf("\n");
			response = ELM_OK_PROMPT;
		}

		printf("%s\n>",response.c_str());

		sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
		sp->writeData(response.c_str());
		sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
		sp->writeData(ELM_PROMPT);
	}

	frame++;
}

//--------------------------------------------------------------
void testApp::draw(){
	ofBackground(0);
	ofSetColor(255);
	ofDrawBitmapString(ofToString(frame),10,20);
}

//--------------------------------------------------------------
void testApp::exit(){
	delete ss;
	delete sp;
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}

void obdsim_freezeframes(struct obdgen_ecu *ecus, int ecu_count) {
	int i;
	for(i=0;i<ecu_count;i++) {
		struct obdgen_ecu *e = &ecus[i];

		/*if(NULL != e->simgen->geterrorcodes) {
			int errorcount;
			int mil;
			errorcount = e->simgen->geterrorcodes(e->dg,
				NULL, 0, &mil);

			if(0 == errorcount) {
				if(e->lasterrorcount > 0) {
					printf("Clearing errors\n");
				}
				e->lasterrorcount = 0;
				e->ffcount = 0;
				continue;
			}

			if(e->lasterrorcount == errorcount) continue;

			// Getting here means there's some new errors
			if(e->ffcount >= OBDSIM_MAXFREEZEFRAMES) {
				fprintf(stderr, "Warning: Ran out of Frames for ecu %i (%s)\nOBDSIM_MAXFREEZEFRAMES=%i\n",
								i, e->simgen->name(),
								OBDSIM_MAXFREEZEFRAMES);
				continue;
			}

			printf("Storing new freezeframe(%i) on ecu %i (%s)\n", e->ffcount, i, e->simgen->name());
			unsigned int j;
			int total_vals = 0;
			for(j=0;j<sizeof(obdcmds_mode1)/sizeof(obdcmds_mode1[0]);j++) {
					e->ff[e->ffcount].valuecount[j] = e->simgen->getvalue(ecus[i].dg,
						0x01, j,
						e->ff[e->ffcount].values[j]+0, e->ff[e->ffcount].values[j]+1,
						e->ff[e->ffcount].values[j]+2, e->ff[e->ffcount].values[j]+3);

					if(e->ff[e->ffcount].valuecount[j] > 0) {
						total_vals++;
					}
			}
			printf("Stored %i vals\n", total_vals);

			e->ffcount++;
			e->lasterrorcount = errorcount;
		}*/
	}
}

int render_obdheader(char *buf, size_t buflen, struct obdiiprotocol *proto,
	struct obdgen_ecu *ecu, unsigned int messagelen, int spaces, int dlc) {

	unsigned int ecuaddress; //< The calculated address of this ecu
	unsigned int segments[4]; //< If the address needs to be split up

	char dlc_str[8] = "\0";
	if(dlc) {
		_snprintf(dlc_str,sizeof(dlc_str),"%01X%s", messagelen+2, spaces?" ":"");
		// printf("dlc_str: '%s'\n", dlc_str);
	}

	switch(proto->headertype) {
		case OBDHEADER_J1850PWM:
			ecuaddress = ecu->ecu_num + 0x10;
			return _snprintf(buf, buflen, "41%s6B%s%02X%s",
				spaces?" ":"",
				spaces?" ":"",
				ecuaddress,
				spaces?" ":"");
			break;
		case OBDHEADER_J1850VPW: // also ISO 9141-2
			ecuaddress = ecu->ecu_num + 0x10;
			return _snprintf(buf, buflen, "48%s6B%s%02X%s",
				spaces?" ":"",
				spaces?" ":"",
				ecuaddress,
				spaces?" ":"");
			break;
		case OBDHEADER_14230:
			ecuaddress = ecu->ecu_num + 0x10;
			return _snprintf(buf, buflen, "%02X%sF1%s%02X%s",
				(unsigned)0x80 | messagelen, spaces?" ":"",
				spaces?" ":"",
				ecuaddress,
				spaces?" ":"");
			break;
		case OBDHEADER_CAN29:
			ecuaddress = ecu->ecu_num + 0x18DAF110;
			segments[0] = (ecuaddress >> 24) & 0xFF;
			segments[1] = (ecuaddress >> 16) & 0xFF;
			segments[2] = (ecuaddress >> 8) & 0xFF;
			segments[3] = (ecuaddress >> 0) & 0xFF;
			return _snprintf(buf, buflen, "%02X%s%02X%s%02X%s%02X%s%02X%s%s",
				segments[0], spaces?" ":"",
				segments[1], spaces?" ":"",
				segments[2], spaces?" ":"",
				segments[3], spaces?" ":"",
				messagelen, spaces?" ":"",
				dlc_str);
			break;
		case OBDHEADER_CAN11:
			ecuaddress = ecu->ecu_num + 0x7E8;
			return _snprintf(buf, buflen, "%03X%s%02X%s%s",
				ecuaddress, spaces?" ":"",
				messagelen, spaces?" ":"",
				dlc_str);
			break;
		case OBDHEADER_NULL:
		default:
			return 0;
			break;
	}
	return _snprintf(buf, buflen, "UNKNOWN%s%s", spaces?" ":"", dlc_str);
}

int parse_ATcmd(struct simsettings *ss, OBDSimPort *sp, char *line, string& response) {
	// This is an AT command

	struct timeval timeouttime; // Used anytime we need a simulated timeout

	int atopt_i; // If they pass an integer option
	char atopt_c; // If they pass a character option
	unsigned int atopt_ui; // For hex values, mostly

	char *at_cmd = line + 2;

	int command_recognised = 0;

	for(; ' ' == *at_cmd; at_cmd++) { // Find the first non-space character in the AT command
	}

	if(1 == sscanf(at_cmd, "AT%i", &atopt_i)) {
		if(atopt_i >=0 && atopt_i <= 2) {
			printf("Adaptive Timing: %i\n", atopt_i);
			ss->e_adaptive = atopt_i;
			command_recognised = 1;
			response = ELM_OK_PROMPT;
		}
	}

	else if(1 == sscanf(at_cmd, "L%i", &atopt_i)) {
		printf("Linefeed %s\n", atopt_i?"enabled":"disabled");
		ss->e_linefeed = atopt_i;
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(1 == sscanf(at_cmd, "H%i", &atopt_i)) {
		printf("Headers %s\n", atopt_i?"enabled":"disabled");
		ss->e_headers = atopt_i;
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(('S' == at_cmd[0] || 'T' == at_cmd[0]) && 'P' == at_cmd[1]) {
		if(0 == set_obdprotocol(at_cmd+2, ss)) {
			command_recognised = 1;
			printf("New Protocol: %s\n", ss->e_protocol->protocol_desc);
			response = ELM_OK_PROMPT;
		}
	}

	else if(1 == sscanf(at_cmd, "S%i", &atopt_i)) {
		printf("Spaces %s\n", atopt_i?"enabled":"disabled");
		ss->e_spaces = atopt_i;
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(1 == sscanf(at_cmd, "E%i", &atopt_i)) {
		printf("Echo %s\n", atopt_i?"enabled":"disabled");
		ss->e_echo = atopt_i;
		sp->setEcho(ss->e_echo);
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(1 == sscanf(at_cmd, "ST%02X", &atopt_ui)) {
		if(0 == atopt_ui) {
			ss->e_timeout = ELM_TIMEOUT;
		} else {
			ss->e_timeout = 4 * atopt_ui;
		}
		printf("Timeout %i\n", ss->e_timeout);
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(1 == sscanf(at_cmd, "@%x", &atopt_ui)) {
		if(1 == atopt_ui) {
			command_recognised = 1;
			response = ss->elm_device;
		} else if(2 == atopt_ui) {
			command_recognised = 1;
			response = ss->device_identifier;
		} else if(3 == atopt_ui) {
			free(ss->device_identifier);
			char *newid = at_cmd+2;
			while(' ' == *newid) newid++;
			ss->device_identifier = strdup(newid);
			printf("Set device identifier to \"%s\"\n", ss->device_identifier);
			command_recognised = 1;
			response = ELM_OK_PROMPT;
		}
	}

	else if(1 == sscanf(at_cmd, "CV%4i", &atopt_i)) {
		ss->e_currentvoltage = (float)atopt_i/100;
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if(0 == strncmp(at_cmd, "RV", 2)) {
		float delta = (float)rand()/(float)RAND_MAX - 0.5f;
		response = ofToString(ss->e_currentvoltage+delta);
		command_recognised = 1;
	}

	else if(0 == strncmp(at_cmd, "DPN", 3)) {
		response = ss->e_autoprotocol?"A":"";
		response += ofToString(ss->e_protocol->protocol_num);
		command_recognised = 1;
	} else if(0 == strncmp(at_cmd, "DP", 2)) {
		response = ss->e_autoprotocol?"Auto, ":"";
		response += ss->e_protocol->protocol_desc;
		command_recognised = 1;
	}

	else if(1 == sscanf(at_cmd, "D%i", &atopt_i)) {
		printf("DLC display %s\n", atopt_i?"enabled":"disabled");
		ss->e_dlc = atopt_i;
		command_recognised = 1;
		response = ELM_OK_PROMPT;
	}

	else if('I' == at_cmd[0]) {
		response = ss->elm_version;
		command_recognised = 1;
	}

	else if('Z' == at_cmd[0] || 0 == strncmp(at_cmd, "WS", 2) || 'D' == at_cmd[0]) {

		if('Z' == at_cmd[0]) {
			printf("Reset\n");
			response = ss->elm_version;
			
			// 10 times the regular timeout, just for want of a number
			timeouttime.tv_sec=0;
			timeouttime.tv_usec=1000l*ss->e_timeout * 10 / (ss->e_adaptive +1);
			select(0,NULL,NULL,NULL,&timeouttime);
		} else if('D' == at_cmd[0]) {
			printf("Defaults\n");
			response = ELM_OK_PROMPT;
		} else {
			printf("Warm Start\n");
			response = ss->elm_version;

			// Wait half as long as a reset
			timeouttime.tv_sec=0;
			timeouttime.tv_usec=1000l*ss->e_timeout * 5 / (ss->e_adaptive +1);
			select(0,NULL,NULL,NULL,&timeouttime);
		}

		obdsim_elmreset(ss);
		sp->setEcho(ss->e_echo);

		command_recognised = 1;
	}

	return command_recognised;
}

int parse_01cmd(struct simsettings *ss, OBDSimPort *sp, char *line, string& response) {
	// This is an AT command

	struct timeval timeouttime; // Used anytime we need a simulated timeout

	int atopt_i; // If they pass an integer option
	char atopt_c; // If they pass a character option
	unsigned int atopt_ui; // For hex values, mostly

	char *at_cmd = line + 2;

	int command_recognised = 0;

	for(; ' ' == *at_cmd; at_cmd++) { // Find the first non-space character in the AT command
	}

	if(0 == strncmp(at_cmd, "0D", 2)) { //SPEED //Metrics
		printf("SPEED %i\n",speed);
		command_recognised = 1;
		response = "41 0D " + ofToHex(speed);
		speed++;
	}
	
	else if(0 == strncmp(at_cmd, "0C", 2)) { //RPM
		printf("RPM %i\n",rpm);
		command_recognised = 1;
		response = "41 0C " + ofToHex(rpm);
		rpm++;
	}
	
	else if(0 == strncmp(at_cmd, "1F", 2)) { //RUNTIME //long
		printf("RUNTIME %i\n",runtime);
		command_recognised = 1;
		response = "41 1F " + ofToHex(runtime);
		runtime++;
	}

	else if(0 == strncmp(at_cmd, "2F", 2)) { //NAFTA
		printf("NAFTA %i\n",nafta);
		command_recognised = 1;
		response = "41 2F " + ofToHex(nafta);
		nafta++;
	}

	else if(0 == strncmp(at_cmd, "05", 2)) { //TEMP //C
		printf("TEMP %i\n",temp);
		command_recognised = 1;
		response = "41 05 " + ofToHex(temp);
		temp++;
	}


	return command_recognised;
}
