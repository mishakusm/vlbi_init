// args


#define NETMAP_WITH_LIBS
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <cstdint>
#include <fstream>
#include <net/ethernet.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <net/netmap_user.h>
#include <arpa/inet.h>
#include <time.h>


#include <fcntl.h>
#include <sys/ioctl.h>


#ifdef __FreeBSD__
#include <sys/endian.h> /* le64toh */
#include <machine/param.h>

#include <pthread_np.h> /* pthread w/ affinity */
#include <sys/cpuset.h> /* cpu_set */
#include <net/if_dl.h>  /* LLADDR */
#endif  /* __FreeBSD__ */

#ifdef __FreeBSD__
#include <net/if_tun.h>
#define TAP_CLONEDEV	"/dev/tap"
#endif /* __FreeBSD */


using namespace std;

#define OPT_PREFETCH	1
#define OPT_ACCESS	2
#define OPT_COPY	4
#define OPT_MEMCPY	8
#define OPT_TS		16	/* add a timestamp */
#define OPT_INDIRECT	32	/* use indirect buffers, tx only */
#define OPT_DUMP	64	/* dump rx/tx traffic */
#define OPT_RUBBISH	256	/* send whatever the buffers contain */
#define OPT_RANDOM_SRC  512
#define OPT_RANDOM_DST  1024
#define OPT_PPS_STATS   2048
#define OPT_UPDATE_CSUM 4096

#define VIRT_HDR_1	10	/* length of a base vnet-hdr */
#define VIRT_HDR_2	12	/* length of the extenede vnet-hdr */
#define VIRT_HDR_MAX	VIRT_HDR_2

#define MAC_ADDRESS_LENGTH 6



struct virt_header 
{
	uint8_t fields[VIRT_HDR_MAX];
};

struct mac_range {
	const char *name;
	struct ether_addr start, end;
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
		pkt->VDIF_Data_Frame.vdif_header[1].to_int = pkt->VDIF_Data_Frame.vdif_header[1].to_int & 0xFE000000;
		number_df = pkt->VDIF_Data_Frame.vdif_header[1].to_int & 0x7FFFFF;
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



int vdif_pkt_gen (pkt_vldi *pkt)
{
    struct netmap_ring *txring;
    txring = (netmap_ring*) malloc (sizeof (netmap_ring));


    u_int cur=txring->cur;    
    u_int  n = nm_ring_space(txring);
//   cout << endl << "HERE" << endl << cur << endl;
//   cout << endl << "HEREE" << endl << txring->slot[cur].buf_idx;

    // copy mac
   // memcpy(pkt->eh.ether_dhost, "\xff\xff\xff\xff\xff\xff", ETHER_ADDR_LEN); // dest
   // memcpy(pkt->eh.ether_shost, "\x08\x00\x27\x4b\x45\x6e", ETHER_ADDR_LEN); // src
    pkt->eh.ether_type = htons(ETHERTYPE_IP); // protocol
 
    for (int rx =0; rx < n; rx++) 
    {
	
    	u_int buf_idx = txring->slot[cur].buf_idx;
  	char* tx_buf = NETMAP_BUF(txring, buf_idx);
	int frame_len = sizeof(pkt);
	memcpy(tx_buf,  pkt, frame_len);
	txring->slot[cur].len = frame_len;
   	txring->cur = nm_ring_next(txring, cur);
	cur = txring->cur;
	
     }
    config_header (pkt);
    free (txring);
	
return 0;
}



int main (int arc, char **argv)
{
	char[20] interface;
	

	clock_t time = 5;
	pkt_vldi *pkt = new pkt_vldi;
	unsigned char dest[MAC_ADDRESS_LENGTH];
	char *mac_address_str_d;
	unsigned char src[MAC_ADDRESS_LENGTH];
	char *mac_address_str_s;
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

	config_header (pkt);
	int ch;
	while ((ch = getopt(arc, argv, "d:s:t:i:")) != -1) 
	{
		switch (ch)
		{	
			
			case 'i':
				interface = optarg;
			break;
			
			case 'd':
				mac_address_str_d = optarg;
				if (strlen(mac_address_str_d) != 17) 
				{
       				 	printf("Invalid MAC address format (destonition). Format expected XX:XX:XX:XX:XX:XX\n");
        				return 1;
   				}
				if (sscanf(mac_address_str_d, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dest[0], &dest[1], &dest[2], &dest[3], &dest[4], &dest[5]) != MAC_ADDRESS_LENGTH) 
				{
        				printf("Error in getting mac!\n");
        				return 2;
    				}
			break;
			case 's':
				mac_address_str_s = optarg;
				if (strlen(mac_address_str_s) != 17) 
				{
       				 	printf("Invalid MAC address format (source). Format expected XX:XX:XX:XX:XX:XX\n");
        				return 3;
   				}
				if (sscanf(mac_address_str_s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &src[0], &src[1], &src[2], &src[3], &src[4], &src[5]) != MAC_ADDRESS_LENGTH) 
				{
        				printf("Error in getting mac!\n");
        				return 4;
				}
			break;
			case 't':
				char *time_str = optarg;
			    	time = (clock_t)atol(time_str);
			break;
			
		}
	}
			if (src == NULL || dest == NULL || interface == NULL) 
			{
      			        fprintf(stderr, "-i, -d and -s options are required.\n");
      				return 5;
   			}
			char[30] nm_interface;
			
			nm_interface = "netmap:";
			strcat(nm_interface, interface);
   			struct nm_desc *nmd;
   			nmd = nm_open(nm_interface, nullptr, NM_OPEN_NO_MMAP, nullptr);
 			if (nmd == nullptr) 
   			{
    				cerr << "Failed to open netmap device" << endl;
    				return 6;
   			}


			for (int i=0; i<MAC_ADDRESS_LENGTH; i++)
			{
				pkt->eh.ether_dhost[i]=dest[i];
				pkt->eh.ether_shost[i]=src[i];
			}
			
		

			clock_t start_time = clock(); // Запоминаем время начала выполнения
    			clock_t current_time;
			
   			do 
			{
				int check = vdif_pkt_gen (pkt);
				if (check!=0)
				{
					cout << "Error in sending packet!";
					return 7;
				}
				current_time = clock();
			}while ((current_time - start_time) / CLOCKS_PER_SEC < time);
			nm_close(nmd);
			
	
return 0;

}
