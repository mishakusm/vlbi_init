#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <cstdint>
#include <fstream>
#include <netinet/ether.h>
#include <netinet/udp.h>
using namespace std;


#define VIRT_HDR_1	10	/* length of a base vnet-hdr */
#define VIRT_HDR_2	12	/* length of the extenede vnet-hdr */
#define VIRT_HDR_MAX	VIRT_HDR_2
struct virt_header 
{
	uint8_t fields[VIRT_HDR_MAX];
};


#define MAX_BODYSIZE	65536

union word
{
    char bytes[4];
    unsigned int to_int;
};


struct pkt_vldi
{
	struct virt_header vh;
	struct ether_header eh;
	struct 
	{
		word vdif_header[8];
		struct udphdr udp;
		uint8_t body[MAX_BODYSIZE];	/* hardwired */
	} VDIF_Data_Frame;

} __attribute__((__packed__));


int fill_pkt_body (pkt_vldi *pkt)
{
	int i=0;
	for (int k = 0; k < MAX_BODYSIZE; k++)
	{
		pkt->VDIF_Data_Frame.body[k]=i;
		i++;
		if (i==255) i=0;
		//printf("%d\n", pkt->VDIF_Data_Frame.body[k]);
	}
	return 0;
}

int set_pkt_header(pkt_vldi *pkt)
{
	ifstream inputFile("head_example", std::ios::binary);
	if (!inputFile)
	{
		cerr << "Unable to open input file" << endl;
		return 1;
	}

	for (int i=0; i<8; ++i)
	{
		inputFile.read(pkt->VDIF_Data_Frame.vdif_header[i].bytes, sizeof(uint32_t));
	}

	inputFile.close();
	return 0;
}

int config_header (pkt_vldi *pkt)
{

	//Parse Frame Number, Seconds, Frame length
	int number_df = pkt->VDIF_Data_Frame.vdif_header[1].to_int & 0x7FFFFF;
	if (number_df > 49999) 
	{
		pkt->VDIF_Data_Frame.vdif_header[1].to_int = pkt->VDIF_Data_Frame.vdif_header[1].to_int & 0xFF000000;
	}

        cout << "Frame Number in second:" << number_df << endl;
        int cur_sec = pkt->VDIF_Data_Frame.vdif_header[0].to_int & 0x3FFFFFFF;
        cout << "Seconds:" << cur_sec << "\t";
        int frame_len = (pkt->VDIF_Data_Frame.vdif_header[2].to_int & 0xffffff) * 8;
        cout << "Frame length:" << frame_len << "\t";



	//Data Frame # within second:
	pkt->VDIF_Data_Frame.vdif_header[1].to_int = pkt->VDIF_Data_Frame.vdif_header[1].to_int + 1;



	return 0;
}



int main ()
{
	pkt_vldi *pkt = new pkt_vldi;

	fill_pkt_body (pkt);

	for (int k = 0; k < MAX_BODYSIZE; k++)
	{
		//printf("%d\n", pkt->VDIF_Data_Frame.body[k]);
	}
	set_pkt_header(pkt);
	for (int i = 0; i <= 8; i++)
	{
		printf("%d\n", pkt->VDIF_Data_Frame.vdif_header[i].to_int);
	}


	pkt->VDIF_Data_Frame.vdif_header[1].to_int = pkt->VDIF_Data_Frame.vdif_header[1].to_int + 50000;
	config_header (pkt);



	getchar();

}