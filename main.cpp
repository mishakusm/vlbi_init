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
int fd = open("/dev/netmap", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/netmap");
        exit(EXIT_FAILURE);
    }

    struct nmreq req;
    bzero(&req, sizeof(req));
    strncpy(req.nr_name, "netmap:eth0", sizeof(req.nr_name));
    req.nr_version = NETMAP_API;
    ioctl(fd, NIOCREGIF, &req);

    struct netmap_if *nifp = NETMAP_IF(&req ,req.nr_offset);
    struct netmap_ring *txring = NETMAP_TXRING(nifp, 0);

/*
    ether_aton_r("ff:ff:ff:ff:ff:ff", pkt->eh.ether_dhost); // dest
    ether_aton_r("08:00:27:4b:45:6e", pkt->eh.ether_shost); // src
*/

	
    // copy mac
    memcpy(pkt->eh.ether_dhost, "\xff\xff\xff\xff\xff\xff", ETHER_ADDR_LEN); // dest
    memcpy(pkt->eh.ether_shost, "\x08\x00\x27\x4b\x45\x6e", ETHER_ADDR_LEN); // src
    pkt->eh.ether_type = htons(ETHERTYPE_IP); // protocol

	
    time_t start_time = time(NULL);
    while (difftime(time(NULL), start_time) < 10) 
    {
   	 int32_t cur = txring->cur;
   	 struct netmap_slot *slot = &txring->slot[cur];

   	 // Put data to slot	
    	memcpy(NETMAP_BUF(txring, slot->buf_idx), pkt, sizeof(struct pkt_vldi));

	    // packet size
    	slot->len = sizeof(struct pkt_vldi);

    	// Curr slot
    	txring->cur = NETMAP_RING_NEXT(txring, cur);

    	// SEND PKT
    	ioctl(fd, NIOCTXSYNC, NULL);
    }

    close(fd);
    return 0;

	
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

	config_header (pkt);

	int menu;
	std::cin >> menu;
	
	if (menu==1)
	{
		vdif_pkt_gen (pkt);
		return 0;
	}
	else
	{
		return 0;
	}

}
