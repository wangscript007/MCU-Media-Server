#include <arpa/inet.h>
#include "log.h"
#include "rtp.h"

static DWORD GetRTCPHeaderLength(rtcp_common_t* header)
{
	return (ntohs(header->length)+1)*4;
}

static void SetRTCPHeaderLength(rtcp_common_t* header,DWORD size)
{
	header->length = htons(size/4-1);
}

void RTCPPacket::Dump()
{
	Debug("\t[RTCPpacket type=%s size=%d/]\n",TypeToString(type),GetSize());
}

void RTCPCompoundPacket::Dump()
{
	Debug("[RTCPCompoundPacket count=%d size=%d]\n",packets.size(),GetSize());
	//For each one
	for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
		//Dump
		(*it)->Dump();
	Debug("[/RTCPCompoundPacket]\n");
}
RTCPCompoundPacket* RTCPCompoundPacket::Parse(BYTE *data,DWORD size)
{
	//Check if it is an RTCP valid header
	if (!IsRTCP(data,size))
		//Exit
		return NULL;
	//Create pacekt
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();
	//Init pointers
	BYTE *buffer = data;
	DWORD bufferLen = size;
	//Parse
	while (bufferLen)
	{
		RTCPPacket *packet = NULL;
		//Get header
		rtcp_common_t* header = (rtcp_common_t*) buffer;
		//Get type
		RTCPPacket::Type type = (RTCPPacket::Type)header->pt;
		//Create new packet
		switch (type)
		{
			case RTCPPacket::SenderReport:
				//Create packet
				packet = new RTCPSenderReport();
				break;
			case RTCPPacket::ReceiverReport:
				//Create packet
				packet = new RTCPReceiverReport();
				break;
			case RTCPPacket::SDES:
				//Create packet
				packet = new RTCPSDES();
				break;
			case RTCPPacket::Bye:
				//Create packet
				packet = new RTCPBye();
				break;
			case RTCPPacket::App:
				//Create packet
				packet = new RTCPApp();
				break;
			case RTCPPacket::RTPFeedback:
				//Create packet
				packet = new RTCPRTPFeedback();
				break;
			case RTCPPacket::PayloadFeedback:
				//Create packet
				packet = new RTCPPayloadFeedback();
				break;
			case RTCPPacket::FullIntraRequest:
				//Create packet
				packet = new RTCPFullIntraRequest();
				break;
			case RTCPPacket::NACK:
				//Create packet
				packet = new RTCPNACK();
				break;
			case RTCPPacket::ExtendedJitterReport:
				//Create packet
				packet = new RTCPExtendedJitterReport();
				break;
			default:
				//Skip
				Debug("-Unknown rtcp packet type [%d]\n",header->pt);
		}
		//Get size of the packet
		DWORD len = GetRTCPHeaderLength(header);
		//Check len
		if (len>bufferLen)
			//error
			return (RTCPCompoundPacket*)Error("Wrong rtcp packet size");
		//parse
		if (packet && packet->Parse(buffer,len))
			//Add packet
			rtcp->AddRTCPacket(packet);
		//Remove size
		bufferLen -= len;
		//Increase pointer
		buffer    += len;
	}

	//Return it
	return rtcp;
}
RTCPSenderReport::RTCPSenderReport() : RTCPPacket(RTCPPacket::SenderReport)
{
	ssrc = 0;
	ntpSec = 0;
	ntpFrac = 0;
	rtpTimestamp = 0;
	packetsSent = 0;
	octectsSent = 0;
}

RTCPSenderReport::~RTCPSenderReport()
{
	for(Reports::iterator it = reports.begin();it!=reports.end();++it)
		delete(*it);
}

void RTCPSenderReport::SetTimestamp(timeval *tv)
{
	/*
	   Wallclock time (absolute date and time) is represented using the
	   timestamp format of the Network Time Protocol (NTP), which is in
	   seconds relative to 0h UTC on 1 January 1900 [4].  The full
	   resolution NTP timestamp is a 64-bit unsigned fixed-point number with
	   the integer part in the first 32 bits and the fractional part in the
	   last 32 bits.  In some fields where a more compact representation is
	   appropriate, only the middle 32 bits are used; that is, the low 16
	   bits of the integer part and the high 16 bits of the fractional part.
	   The high 16 bits of the integer part must be determined
	   independently.
	 */

	//Convert from ecpoch (JAN_1970) to NTP (JAN 1900);
	SetNTPSec(tv->tv_sec + 2208988800UL);
	//Convert microsecods to 32 bits fraction
	SetNTPFrac(tv->tv_usec*4294.967296);
}

void RTCPSenderReport::GetTimestamp(timeval *tv) const
{
	//Convert to epcoh JAN_1970
	tv->tv_sec = ntpSec - 2208988800UL;
	//Add fraction of
	tv->tv_usec = ntpFrac/4294.967296;
}

QWORD RTCPSenderReport::GetTimestamp() const
{
	//Convert to epcoh JAN_1970
	QWORD ts = ntpSec - 2208988800UL;
	//convert to microseconds
	ts *=1E6;
	//Add fraction
	ts += ntpFrac/4294.967296;
	//Return it
	return ts;
}

void RTCPSenderReport::Dump()
{
	Debug("\t[RTCPSenderReport ssrc=%u count=%u \n",ssrc,reports.size());
	Debug("\t\tntpSec=%u\n"		,ntpSec);
	Debug("\t\tntpFrac=%u\n"	,ntpFrac);
	Debug("\t\trtpTimestamp=%u\n"	,rtpTimestamp);
	Debug("\t\tpacketsSent=%u\n"	,packetsSent);
	Debug("\t\toctectsSent=%u\n"	,octectsSent);
	if (reports.size())
	{
		Debug("\t]\n");
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSenderReport]\n");
	} else
		Debug("\t/]\n");
}

DWORD RTCPSenderReport::GetSize()
{
	return sizeof(rtcp_common_t)+24+24*reports.size();
}

DWORD RTCPSenderReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get info
	ssrc		= get4(data,len);
	ntpSec		= get4(data,len+4);
	ntpFrac		= get4(data,len+8);
	rtpTimestamp	= get4(data,len+12);
	packetsSent	= get4(data,len+16);
	octectsSent	= get4(data,len+20);
	//Move forward
	len += 24;
	//for each
	for(int i=0;i<header->count&&size>=len+24;i++)
	{
		//New report
		RTCPReport* report = new RTCPReport();
		//parse
		len += report->Parse(data+len,size-len);
		//Add it
		AddReport(report);
	}
	
	//Return total size
	return len;
}

DWORD RTCPSenderReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSenderReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= reports.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//Set values
	set4(data,len,ssrc);
	set4(data,len+4,ntpSec);
	set4(data,len+8,ntpFrac);
	set4(data,len+12,rtpTimestamp);
	set4(data,len+16,packetsSent);
	set4(data,len+20,octectsSent);
	//Next
	len += 24;
	//for each
	for(int i=0;i<header->count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}

RTCPReceiverReport::RTCPReceiverReport() : RTCPPacket(RTCPPacket::ReceiverReport)
{

}

RTCPReceiverReport::~RTCPReceiverReport()
{
	for(Reports::iterator it = reports.begin();it!=reports.end();++it)
		delete(*it);
}

void RTCPReceiverReport::Dump()
{
	if (reports.size())
	{
		Debug("\t[RTCPReceiverReport ssrc=%d count=%u]\n",ssrc,reports.size());
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPReceiverReport]\n");
	} else
		Debug("\t[RTCPReceiverReport ssrc=%d]\n",ssrc);
}

DWORD RTCPReceiverReport::GetSize()
{
	return sizeof(rtcp_common_t)+4+24*reports.size();
}

DWORD RTCPReceiverReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get info
	ssrc = get4(data,len);
	//Move forward
	len += 4;
	//for each
	for(int i=0;i<header->count&&size>=len+24;i++)
	{
		//New report
		RTCPReport* report = new RTCPReport();
		//parse
		len += report->Parse(data+len,size-len);
		//Add it
		AddReport(report);
	}

	//Return total size
	return len;
}

DWORD RTCPReceiverReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPReceiverReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= reports.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//Set values
	set4(data,len,ssrc);
	//Next
	len += 4;
	//for each
	for(int i=0;i<header->count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}

RTCPBye::RTCPBye() : RTCPPacket(RTCPPacket::Bye)
{
	//No reason
	reason = NULL;
}

RTCPBye::~RTCPBye()
{
	//Free
	if (reason)
		free(reason);
}
DWORD RTCPBye::GetSize()
{
	DWORD len = sizeof(rtcp_common_t)+4*ssrcs.size();
	if (reason)
		len += strlen(reason)+1;
	return len;
}

DWORD RTCPBye::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
	{
		//Get ssrc
		ssrcs.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Check if more preseng
	if (packetSize>len)
	{
		//Get len or reason
		DWORD n = data[len];
		//Allocate mem
		reason = (char*)malloc(n+1);
		//Copy
		memcpy(reason,data+len+1,n);
		//End it
		reason[n] = 0;
		//Move
		len += n+1;
	}

	//Return total size
	return len;
}

DWORD RTCPBye::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPBye invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= ssrcs.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<ssrcs.size();i++)
	{
		//Set ssrc
		set4(data,len,ssrcs[i]);
		//skip
		len += 4;
	}
	//Optional reason
	if (reason)
	{
		//Set reason length
		data[len] = strlen(reason);

		//Copy reason
		memcpy(data+len+1,reason,strlen(reason));

		//Add len
		len +=strlen(reason)+1;
	}

	//return
	return len;
}



RTCPExtendedJitterReport::RTCPExtendedJitterReport() : RTCPPacket(RTCPPacket::ExtendedJitterReport)
{
}

DWORD RTCPExtendedJitterReport::GetSize()
{
	return sizeof(rtcp_common_t)+4*jitters.size();
}

DWORD RTCPExtendedJitterReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
	{
		//Get ssrc
		jitters.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Return total size
	return len;
}

DWORD RTCPExtendedJitterReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPExtendedJitterReport invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= jitters.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<jitters.size();i++)
	{
		//Set ssrc
		set4(data,len,jitters[i]);
		//skip
		len += 4;
	}
	//return
	return len;
}
RTCPApp::RTCPApp() : RTCPPacket(RTCPPacket::App)
{
	data = NULL;
	size = 0;
	subtype = 0;
}

RTCPApp::~RTCPApp()
{
	if (data)
		free(data);
}

DWORD RTCPApp::GetSize()
{
	return sizeof(rtcp_common_t)+8+size;
}

DWORD RTCPApp::Serialize(BYTE* data, DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPApp invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= subtype;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Copy
	set4(data,len,ssrc);
	//Move
	len += 4;
	//Copy name
	memcpy(data+len,name,4);
	//Inc len
	len += 4;
	//Copy
	memcpy(data+len,this->data,this->size);
	//add it
	len += this->size;
	//return it
	return len;
}

DWORD RTCPApp::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get packet size
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	subtype = header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrc
	ssrc = get4(data,len);
	//Move
	len += 4;
	//Copy name
	memcpy(name,data+len,4);
	//Skip
	len += 4;
	//Set size
	this->size = packetSize-len;
	//Allocate mem
	this->data = (BYTE*)malloc(this->size);
	//Copy data
	memcpy(this->data,data+len,this->size);
	//Skip
	len += this->size;
	//Copy
	return len;
}

RTCPRTPFeedback::RTCPRTPFeedback() : RTCPPacket(RTCPPacket::RTPFeedback)
{

}
RTCPRTPFeedback::~RTCPRTPFeedback()
{
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//delete it
		delete(*it);
}

DWORD RTCPRTPFeedback::GetSize()
{
	DWORD len = 8+sizeof(rtcp_common_t);
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPRTPFeedback::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get size decalred in header
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	feedbackType = (FeedbackType)header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	senderSSRC = get4(data,len);
	mediaSSRC = get4(data,len+4);
	//skip fields
	len += 8;
	//While we have more
	while (len<packetSize)
	{
		Field *field = NULL;
		//Depending on the type
		switch(feedbackType)
		{
			case NACK:
				field = new NACKField();
				break;
			case TempMaxMediaStreamBitrateRequest:
			case TempMaxMediaStreamBitrateNotification:
				field = new TempMaxMediaStreamBitrateField();
				break;
			default:
				return Error("Unknown RTCPRTPFeedback type [%d]\n",header->count);
		}
		//Parse field
		DWORD parsed = field->Parse(data+len,packetSize-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		fields.push_back(field);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len+12;
}

DWORD RTCPRTPFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPRTPFeedback invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= feedbackType;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,senderSSRC);
	set4(data,len+4,mediaSSRC);
	//Inclrease len
	len += 8;
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//Serialize it
		len+=(*it)->Serialize(data+len,size-len);
	//Retrun writed data len
	return len;
}


RTCPPayloadFeedback::RTCPPayloadFeedback() : RTCPPacket(RTCPPacket::PayloadFeedback)
{

}

RTCPPayloadFeedback::~RTCPPayloadFeedback()
{
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//delete it
		delete(*it);
}

void RTCPPayloadFeedback::Dump()
{
	Debug("\t[RTCPPacket PayloadFeedback %s]\n",TypeToString(feedbackType));
	for (int i=0;i<fields.size();i++)
	{
		
		//Check type
		switch(feedbackType)
		{
			case RTCPPayloadFeedback::PictureLossIndication:
			case RTCPPayloadFeedback::FullIntraRequest:
			case RTCPPayloadFeedback::SliceLossIndication:
			case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
			case RTCPPayloadFeedback::VideoBackChannelMessage:
				break;
			case RTCPPayloadFeedback:: ApplicationLayerFeeedbackMessage:
			{
				//Get field
				ApplicationLayerFeeedbackField* field = (ApplicationLayerFeeedbackField*)fields[i];
				//Dump it
				::Dump(field->payload,field->length);
				break;
			}
		}
	}
	Debug("\t[/RTCPPacket PayloadFeedback %s]\n",TypeToString(feedbackType));
}
DWORD RTCPPayloadFeedback::GetSize()
{
	DWORD len = 8+sizeof(rtcp_common_t);
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPPayloadFeedback::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get packet size
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	feedbackType = (FeedbackType)header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	senderSSRC = get4(data,len);
	mediaSSRC = get4(data,len+4);
	//skip fields
	len += 8;
	//While we have more
	while (len<packetSize)
	{
		Field *field = NULL;
		//Depending on the type
		switch(feedbackType)
		{
			case PictureLossIndication:
				return Error("PictureLossIndication with body\n");
			case SliceLossIndication:
				field = new SliceLossIndicationField();
				break;
			case ReferencePictureSelectionIndication:
				field = new ReferencePictureSelectionField();
				break;
			case FullIntraRequest:
				field = new FullIntraRequestField();
				break;
			case TemporalSpatialTradeOffRequest:
			case TemporalSpatialTradeOffNotification:
				field = new TemporalSpatialTradeOffField();
				break;
			case VideoBackChannelMessage:
				field = new VideoBackChannelMessageField();
				break;
			case ApplicationLayerFeeedbackMessage:
				field = new ApplicationLayerFeeedbackField();
				break;
			default:
				return Error("Unknown RTCPPayloadFeedback type [%d]\n",header->count);
		}
		//Parse field
		DWORD parsed = field->Parse(data+len,packetSize-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		fields.push_back(field);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPPayloadFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPPayloadFeedback invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= feedbackType;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,senderSSRC);
	set4(data,len+4,mediaSSRC);
	//Inclrease len
	len += 8;
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//Serialize it
		len+=(*it)->Serialize(data+len,size-len);
	//Retrun writed data len
	return len;
}

RTCPFullIntraRequest::RTCPFullIntraRequest() : RTCPPacket(RTCPPacket::FullIntraRequest)
{

}

DWORD RTCPFullIntraRequest::GetSize()
{
	return sizeof(rtcp_common_t)+4;
}

DWORD RTCPFullIntraRequest::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	ssrc = get4(data,len);
	//Return consumed len
	return len+4;
}

DWORD RTCPFullIntraRequest::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPFullIntraRequest invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= 0;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,ssrc);
	//Retrun writed data len
	return len+4;
}

RTCPNACK::RTCPNACK() : RTCPPacket(RTCPPacket::NACK)
{

}

DWORD RTCPNACK::GetSize()
{
	return sizeof(rtcp_common_t)+8;
}

DWORD RTCPNACK::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	ssrc = get4(data,len);
	fsn = get2(data,len+4);
	blp = get2(data,len+2);
	//Return consumed len
	return len+8;
}

DWORD RTCPNACK::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPNACK invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= 0;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,ssrc);
	set2(data,len+4,fsn);
	set2(data,len+6,blp);
	//Retrun writed data len
	return len+8;
}

RTCPSDES::RTCPSDES() : RTCPPacket(RTCPPacket::SDES)
{

}
RTCPSDES::~RTCPSDES()
{
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		delete(*it);
}

void RTCPSDES::Dump()
{
	
	if (descriptions.size())
	{
		Debug("\t[RTCPSDES count=%u]\n",descriptions.size());
		for(Descriptions::iterator it = descriptions.begin();it!=descriptions.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSDES]\n");
	} else
		Debug("\t[RTCPSDES/]\n");
}
DWORD RTCPSDES::GetSize()
{
	DWORD len = sizeof(rtcp_common_t);
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}
DWORD RTCPSDES::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Parse fields
	DWORD i = 0;
	//While we have
	while (size>len && i<header->count)
	{
		Description *desc = new Description();
		//Parse field
		DWORD parsed = desc->Parse(data+len,size-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		descriptions.push_back(desc);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPSDES::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= descriptions.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//Serilize it
		len += (*it)->Serialize(data+len,size-len);

	//Return
	return len;
 }


RTCPSDES::Description::Description()
{

}
RTCPSDES::Description::Description(DWORD ssrc)
{
	this->ssrc = ssrc;
}
RTCPSDES::Description::~Description()
{
	for (Items::iterator it=items.begin();it!=items.end();++it)
		delete(*it);
}
void RTCPSDES::Description::Dump()
{
	if (items.size())
	{
		Debug("\t\t[Description ssrc=%u count=%u\n",ssrc,items.size());
		for(Items::iterator it=items.begin();it!=items.end();++it)
			Debug("\t\t\t[%s '%.*s'/]\n",RTCPSDES::Item::TypeToString((*it)->GetType()),(*it)->GetSize(),(*it)->GetData());
		Debug("\t\t[/Description]\n");
	} else
		Debug("\t\t[Description ssrc=%x/]\n",ssrc);
}

DWORD RTCPSDES::Description::GetSize()
{
	DWORD len = 4;
	//For each field
	for (Items::iterator it=items.begin();it!=items.end();++it)
		//add data size and header
		len += (*it)->GetSize()+2;
	//ADD end
	len+=1;
	//Return
	return pad32(len);
}
DWORD RTCPSDES::Description::Parse(BYTE* data,DWORD size)
{
	//Check size
	if (size<4)
		//Exit
		return 0;
	//Get ssrc
	ssrc = get4(data,0);
	//Skip ssrc
	DWORD len = 4;
	//While we have
	while (size>len+2 && data[len])
	{
		//Get tvalues
		RTCPSDES::Item::Type type = (RTCPSDES::Item::Type)data[len];
		BYTE length = data[len+1];
		//Check size
		if (len+2+length>size)
			//Error
			return 0;
		//Add item
		items.push_back( new Item(type,data+len+2,length));
		//Move
		len += length+2;
	}
	//Skip last
	len++;
	//Return consumed len
	return pad32(len);
}

DWORD RTCPSDES::Description::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES Description invalid size\n");
	//Set ssrc
	set4(data,0,ssrc);
	//Skip headder
	DWORD len = 4;
	//For each field
	for (Items::iterator it=items.begin();it!=items.end();++it)
	{
		//Get item
		Item *item = (*it);
		//Serilize it
		data[len++] = item->GetType();
		data[len++] = item->GetSize();
		//Copy data
		memcpy(data+len,item->GetData(),item->GetSize());
		//Move
		len += item->GetSize();
	}
	//Add null item
	data[len++] = 0;
	//Append nulls till pading
	memset(data+len,0,pad32(len)-len);
	//Return padded size
	return pad32(len);
 }

RTPRedundantPacket::RTPRedundantPacket(MediaFrame::Type media,BYTE *data,DWORD size) : RTPTimedPacket(media,data,size)
{
	//NO primary data yet
	primaryCodec = 0;
	primaryType = 0;
	primaryData = NULL;
	primarySize = 0;
	
	//Number of bytes to skip of text until primary data
	WORD skip = 0;

	//The the payload
	BYTE *payload = GetMediaData();

	//redundant counter
	WORD i = 0;

	//Check if it is the last
	bool last = !(payload[i]>>7);

	//Read redundant headers
	while(!last)
	{
		//Check it
		/*
		    0                   1                    2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |F|   block PT  |  timestamp offset         |   block length    |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   F: 1 bit First bit in header indicates whether another header block
		       follows.  If 1 further header blocks follow, if 0 this is the
		       last header block.

		   block PT: 7 bits RTP payload type for this block.

		   timestamp offset:  14 bits Unsigned offset of timestamp of this block
		       relative to timestamp given in RTP header.  The use of an unsigned
		       offset implies that redundant payload must be sent after the primary
		       payload, and is hence a time to be subtracted from the current
		       timestamp to determine the timestamp of the payload for which this
		       block is the redundancy.

		   block length:  10 bits Length in bytes of the corresponding payload
		       block excluding header.

		 */

		//Get Type
		BYTE type = payload[i++];
		//Get offset
		WORD offset = payload[i++];
		offset = offset <<6 | payload[i]>>2;
		//Get size
		WORD size = payload[i++] & 0x03;
		size = size <<6 | payload[i++];
		//Append new red header
		headers.push_back(RedHeader(type,offset,skip,size));
		//Skip the redundant payload
		skip += size;
		//Check if it is the last
		last = !(payload[i]>>7);
	}
	//Get primaty type
	primaryType = payload[i] & 0x7F;
	//Skip it
	i++;
	//Get redundant payload
	redundantData = payload+i;
	//Get prymary payload
	primaryData = redundantData+skip;
	//Get size of primary payload
	primarySize = GetMediaLength()-i-skip;
}

RTPTimedPacket* RTPRedundantPacket::CreatePrimaryPacket()
{
	//Create new pacekt
	RTPTimedPacket* packet = new RTPTimedPacket(GetMedia(),primaryCodec,primaryType);
	//Set attributes
	packet->SetClockRate(GetClockRate());
	packet->SetMark(GetMark());
	packet->SetSeqNum(GetSeqNum());
	packet->SetSeqCycles(GetSeqCycles());
	packet->SetTimestamp(GetTimestamp());
	packet->SetSSRC(GetSSRC());
	//Set paylaod
	packet->SetPayload(primaryData,primarySize);
	//Set time
	packet->SetTime(packet->GetTime());
	//Return it
	return packet;
}
