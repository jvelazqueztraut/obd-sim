#include "testApp.h"
#include "windowssimport.h"
#include <Winsock2.h>

/// This is the elm prompt
#define ELM_PROMPT ">"

/// ELM "don't know" prompt
#define ELM_QUERY_PROMPT "?"

/// ELM "OK" prompt
#define ELM_OK_PROMPT "OK"

/// ELM "NO DATA" prompt
#define ELM_NODATA_PROMPT "NO DATA"

char *line; // Single line from the other end of the device
char previousline[1024] = "GARBAGE"; // Blank lines mean re-run previous command

const char *newline_cr = "\r";
const char *newline_crlf = "\r\n";

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

	s->device_identifier = strdup("ChunkyKs");
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
	// Begin main loop by idling for OBDSIM_SLEEPTIME ms
	struct timeval starttime; // start time through loop
	struct timeval endtime; // end time through loop
	struct timeval selecttime; // =endtime-starttime [for select()]

	struct timeval timeouttime; // Used anytime we need a simulated timeout
	// Now the actual choise-response thing
	line = sp->readLine(); // This is the input line
	char response[1024]; // This is the response

	if(NULL != line){
		if(0 == strlen(line)) {
			line = previousline;
		} else {
			strncpy(previousline, line, sizeof(previousline));
		}

		//benchmarkcounttotal++;

		for(int i=strlen(line)-1;i>=0;i--) { // Strlen is expensive, kids.
			line[i] = toupper(line[i]);
		}

		printf("obdsim got request: %s\n", line);

		if(NULL != strstr(line, "EXIT")) {
			printf("Received EXIT via serial port. Sim Exiting\n");
			return;
		}

		// If we recognised the command
		int command_recognised = 0;

		if('A' == line[0] && 'T' == line[1]) {

			//command_recognised = parse_ATcmd(ss,sp,line,response,sizeof(response));

			if(0 == command_recognised) {
				_snprintf(response, sizeof(response), "%s", ELM_QUERY_PROMPT);
			}

			sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
			sp->writeData(response);
			sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
			sp->writeData(ELM_PROMPT);
		}

		int num_vals_read; // Number of values parsed from the sscanf line
		int vals[3]; // Read up to three vals
		num_vals_read = sscanf(line, "%02x %02x %x", &vals[0], &vals[1], &vals[2]);

		int responsecount = 0;

		// Every time we check an ecu, we accumulate time from ecu delays [ms]
		long accumulated_time = 0;
		// Part of accumulating time is finding out how many ECUs have replied
		int ecu_replycount = 0;

		sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);

		// There has *got* to be a better way to do the following complete mess

		if(num_vals_read <= 0) { // Couldn't parse

			_snprintf(response, sizeof(response), "%s", ELM_QUERY_PROMPT);
			sp->writeData(response);
			sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
			responsecount++;
		} else if(num_vals_read == 1) { // Only got one valid thing [assume it's mode]
			/*
			if(0x03 == vals[0] || 0x07 == vals[0]) { // Get error codes
				unsigned int errorcodes[20];
				for(int i=0;i<ss->ecu_count;i++) {
					if(NULL != ss->ecus[i].simgen->geterrorcodes) {
						int errorcount;
						int mil = 0;
						errorcount = ss->ecus[i].simgen->geterrorcodes(ss->ecus[i].dg,
							errorcodes, (sizeof(errorcodes)/sizeof(errorcodes[0]))/2, &mil);

						if(0 == errorcount) continue;

						char header[16] = "\0";
						if(ss->e_headers) {
							render_obdheader(header, sizeof(header), ss->e_protocol, &ss->ecus[i], 7, ss->e_spaces, ss->e_dlc);
						}

						int j;
						for(j=0;j<errorcount;j+=3) {
							char shortbuf[32];
							snprintf(shortbuf, sizeof(shortbuf), "%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
									ss->e_spaces?" ":"", errorcodes[j*2],
									ss->e_spaces?" ":"", errorcodes[j*2+1],

									ss->e_spaces?" ":"", (errorcount-j)>1?errorcodes[(j+1)*2]:0x00,
									ss->e_spaces?" ":"", (errorcount-j)>1?errorcodes[(j+1)*2+1]:0x00,

									ss->e_spaces?" ":"", (errorcount-j)>2?errorcodes[(j+2)*2]:0x00,
									ss->e_spaces?" ":"", (errorcount-j)>2?errorcodes[(j+2)*2+1]:0x00
									);
							// printf("shortbuf: '%s'   i: %i\n", shortbuf, abcd[i]);
							snprintf(response, sizeof(response), "%s%02X%s",
									header, 0x43, shortbuf);
							sp->writeData(response);
							sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
							responsecount++;
						}
					}
				}
			} else if(0x04 == vals[0]) { // Reset error codes
				for(int i=0;i<ss->ecu_count;i++) {
					if(NULL != ss->ecus[i].simgen->clearerrorcodes) {
						ss->ecus[i].simgen->clearerrorcodes(ss->ecus[i].dg);
					}
				}
				snprintf(response, sizeof(response), ELM_OK_PROMPT);
				sp->writeData(response);
				sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
				responsecount++;
			} else { // Can't do anything with one value, in modes other three or four
				snprintf(response, sizeof(response), "%s", ELM_QUERY_PROMPT);
				sp->writeData(response);
				sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
				responsecount++;
			}*/
		} else { // Two or more vals  mode0x01 => mode,pid[,possible optimisation]
								// mode0x02 => mode,pid,frame

			struct obdservicecmd *cmd = obdGetCmdForPID(vals[1]);
			if(NULL == cmd) {
				_snprintf(response, sizeof(response), "%s", ELM_QUERY_PROMPT);
				sp->writeData(response);
				sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
				responsecount++;
			} else if(0x02 == vals[0]) {
				// Freeze frame
				for(int i=0;i<ss->ecu_count;i++) {
					int frame = 0;
					if(num_vals_read > 2) {
						// Third value is the frame
						frame = vals[2];
					}
					if(frame < OBDSIM_MAXFREEZEFRAMES && frame <= ss->ecus[i].ffcount) {
						// Don't understand frames higher than this
						char ffmessage[256] = "\0";
						struct freezeframe *ff = &(ss->ecus[i].ff[frame]);
						int count = ff->valuecount[vals[1]];
						int messagelen = count + 3; // Mode, PID, Frame

						// Or could probably just strncat some or something
						switch(count) {
							case 1:
								_snprintf(ffmessage, sizeof(ffmessage),"%02X%s%02X%s%02X%s%02X",
									vals[0], ss->e_spaces?" ":"",
									vals[1], ss->e_spaces?" ":"",
									frame, ss->e_spaces?" ":"",
									ss->ecus[i].ff[frame].values[vals[1]][0]);
								break;
							case 2:
								_snprintf(ffmessage, sizeof(ffmessage),"%02X%s%02X%s%02X%s%02X%s%02X",
									vals[0], ss->e_spaces?" ":"",
									vals[1], ss->e_spaces?" ":"",
									frame, ss->e_spaces?" ":"",
									ff->values[vals[1]][0], ss->e_spaces?" ":"",
									ff->values[vals[1]][1]);
								break;
							case 3:
								_snprintf(ffmessage, sizeof(ffmessage),"%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
									vals[0], ss->e_spaces?" ":"",
									vals[1], ss->e_spaces?" ":"",
									frame, ss->e_spaces?" ":"",
									ff->values[vals[1]][0], ss->e_spaces?" ":"",
									ff->values[vals[1]][1], ss->e_spaces?" ":"",
									ff->values[vals[1]][2]);
								break;
							case 4:
								_snprintf(ffmessage, sizeof(ffmessage),"%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
									vals[0], ss->e_spaces?" ":"",
									vals[1], ss->e_spaces?" ":"",
									frame, ss->e_spaces?" ":"",
									ff->values[vals[1]][0], ss->e_spaces?" ":"",
									ff->values[vals[1]][1], ss->e_spaces?" ":"",
									ff->values[vals[1]][2], ss->e_spaces?" ":"",
									ff->values[vals[1]][3]);
								break;
							case 0:
							default:
								// NO DATA
								break;
						}
						if(count > 0) {
							char header[16] = "\0";
							if(ss->e_headers) {
								render_obdheader(header, sizeof(header), ss->e_protocol, &ss->ecus[i], 7, ss->e_spaces, ss->e_dlc);
							}
							_snprintf(response, sizeof(response), "%s%s", header, ffmessage);
							sp->writeData(response);
							sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
							responsecount++;
						}
					}

				}
			} else if(0x01 != vals[0]) {
				// Eventually, modes other than 1 should move to the generators
				//  but for now, respond NO DATA
			} else {

				// Here's the meat & potatoes of the whole application

				for(int i=0;i<ss->ecu_count;i++) {
					unsigned int abcd[8];
					struct obdgen_ecu *e = ss->ecudelays[i].ecu;

					if(accumulated_time+ss->ecudelays[i].delay > ss->e_timeout) {
						printf("Timeout waiting for ecu %i. [%i > %i]\n",
							e->ecu_num, e->customdelay, ss->e_timeout);
						continue;
					}
					
					accumulated_time += ss->ecudelays[i].delay;
					ecu_replycount++;

					timeouttime.tv_sec=0;
					timeouttime.tv_usec=1000l*ss->ecudelays[i].delay;
					select(0,NULL,NULL,NULL,&timeouttime);

					//int count = e->simgen->getvalue(e->dg,
					//				vals[0], vals[1],
					///				abcd+0, abcd+1, abcd+2, abcd+3);
					// fprintf(stderr, "ecu %i count %i\n", i, count);

					// printf("Returning %i values for %02X %02X\n", count, vals[0], vals[1]);
					int count = 1;
					/*if(-1 == count) {
						mustexit = 1;
						break;
					}*/

					if(0 < count) {
						char header[16] = "\0";
						if(ss->e_headers) {
							render_obdheader(header, sizeof(header), ss->e_protocol, e, count+2, ss->e_spaces, ss->e_dlc);
						}
						int j;
						char shortbuf[64];
						_snprintf(response, sizeof(response), "%s%02X%s%02X",
									header,
									vals[0]+0x40, ss->e_spaces?" ":"", vals[1]);
						int checksum = 0;
						for(int j=0;j<count && j<sizeof(abcd)/sizeof(abcd[0]);j++) {
							checksum+=abcd[j];
							_snprintf(shortbuf, sizeof(shortbuf), "%s%02X",
									ss->e_spaces?" ":"", abcd[j]);
							// printf("shortbuf: '%s'   j: %i\n", shortbuf, abcd[j]);
							strcat(response, shortbuf);
						}
						if(ss->e_headers) {
										/* &&
								(ss->e_protocol->headertype == OBDHEADER_CAN29 ||
								ss->e_protocol->headertype == OBDHEADER_CAN11)) { */
							_snprintf(shortbuf, sizeof(shortbuf), "%s%02X",
									ss->e_spaces?" ":"", checksum&0xFF);
							strcat(response, shortbuf);
						}

						sp->writeData(response);
						sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
						responsecount++;
					}
				}
			}
		}

		// Only need a timeout if we didn't get replies from all the ECUs
		if(ecu_replycount<ss->ecu_count || (0x01 == vals[0] && num_vals_read <= 2)) {
			timeouttime.tv_sec=0;
			timeouttime.tv_usec=1000l*(ss->e_timeout - accumulated_time);
			select(0,NULL,NULL,NULL,&timeouttime);
		}
		if(0 >= responsecount) {
			sp->writeData(ELM_NODATA_PROMPT);
			sp->writeData(ss->e_linefeed?newline_crlf:newline_cr);
		} else {
			//benchmarkcountgood++;
		}
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

int parse_ATcmd(struct simsettings *ss, OBDSimPort *sp, char *line, char *response, size_t n) {
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
			_snprintf(response, n, "%s", ELM_OK_PROMPT);
		}
	}

	else if(1 == sscanf(at_cmd, "L%i", &atopt_i)) {
		printf("Linefeed %s\n", atopt_i?"enabled":"disabled");
		ss->e_linefeed = atopt_i;
		command_recognised = 1;
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
	}

	else if(1 == sscanf(at_cmd, "H%i", &atopt_i)) {
		printf("Headers %s\n", atopt_i?"enabled":"disabled");
		ss->e_headers = atopt_i;
		command_recognised = 1;
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
	}

	else if(('S' == at_cmd[0] || 'T' == at_cmd[0]) && 'P' == at_cmd[1]) {
		if(0 == set_obdprotocol(at_cmd+2, ss)) {
			command_recognised = 1;
			printf("New Protocol: %s\n", ss->e_protocol->protocol_desc);
			_snprintf(response, n, "%s", ELM_OK_PROMPT);
		}
	}

	else if(1 == sscanf(at_cmd, "S%i", &atopt_i)) {
		printf("Spaces %s\n", atopt_i?"enabled":"disabled");
		ss->e_spaces = atopt_i;
		command_recognised = 1;
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
	}

	else if(1 == sscanf(at_cmd, "E%i", &atopt_i)) {
		printf("Echo %s\n", atopt_i?"enabled":"disabled");
		ss->e_echo = atopt_i;
		sp->setEcho(ss->e_echo);
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
		command_recognised = 1;
	}

	else if(1 == sscanf(at_cmd, "ST%02X", &atopt_ui)) {
		if(0 == atopt_ui) {
			ss->e_timeout = ELM_TIMEOUT;
		} else {
			ss->e_timeout = 4 * atopt_ui;
		}
		printf("Timeout %i\n", ss->e_timeout);
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
		command_recognised = 1;
	}

	else if(1 == sscanf(at_cmd, "@%x", &atopt_ui)) {
		if(1 == atopt_ui) {
			_snprintf(response, n, "%s", ss->elm_device);
			command_recognised = 1;
		} else if(2 == atopt_ui) {
			_snprintf(response, n, "%s", ss->device_identifier);
			command_recognised = 1;
		} else if(3 == atopt_ui) {
			_snprintf(response, n, "%s", ELM_OK_PROMPT);
			free(ss->device_identifier);
			char *newid = at_cmd+2;
			while(' ' == *newid) newid++;
			ss->device_identifier = strdup(newid);
			printf("Set device identifier to \"%s\"\n", ss->device_identifier);
			command_recognised = 1;
		}
	}

	else if(1 == sscanf(at_cmd, "CV%4i", &atopt_i)) {
		ss->e_currentvoltage = (float)atopt_i/100;
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
		command_recognised = 1;
	}

	else if(0 == strncmp(at_cmd, "RV", 2)) {
		float delta = (float)rand()/(float)RAND_MAX - 0.5f;
		_snprintf(response, n, "%.1f", ss->e_currentvoltage+delta);
		command_recognised = 1;
	}

	else if(0 == strncmp(at_cmd, "DPN", 3)) {
		_snprintf(response, n, "%s%c", ss->e_autoprotocol?"A":"", ss->e_protocol->protocol_num);
		command_recognised = 1;
	} else if(0 == strncmp(at_cmd, "DP", 2)) {
		_snprintf(response, n, "%s%s", ss->e_autoprotocol?"Auto, ":"", ss->e_protocol->protocol_desc);
		command_recognised = 1;
	}

	else if(1 == sscanf(at_cmd, "D%i", &atopt_i)) {
		printf("DLC display %s\n", atopt_i?"enabled":"disabled");
		ss->e_dlc = atopt_i;
		command_recognised = 1;
		_snprintf(response, n, "%s", ELM_OK_PROMPT);
	}

	else if('I' == at_cmd[0]) {
		_snprintf(response, n, "%s", ss->elm_version);
		command_recognised = 1;
	}

	else if('Z' == at_cmd[0] || 0 == strncmp(at_cmd, "WS", 2) || 'D' == at_cmd[0]) {

		if('Z' == at_cmd[0]) {
			printf("Reset\n");
			_snprintf(response, n, "%s", ss->elm_version);
			
			// 10 times the regular timeout, just for want of a number
			timeouttime.tv_sec=0;
			timeouttime.tv_usec=1000l*ss->e_timeout * 10 / (ss->e_adaptive +1);
			select(0,NULL,NULL,NULL,&timeouttime);
		} else if('D' == at_cmd[0]) {
			printf("Defaults\n");
			_snprintf(response, n, "%s", ELM_OK_PROMPT);
		} else {
			printf("Warm Start\n");
			_snprintf(response, n, "%s", ss->elm_version);

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
