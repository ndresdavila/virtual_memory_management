#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

//Helper variables:

//Counter of pages in Arbeitsspeicher: for a memory of 65536 bytes (Arbeitsspeicher) divided by pages of 4KB each (4096 bytes), we need a max. of 65536/4096 = 16 pages
uint8_t ram_page_number = 0;
//Counter used for 'last recently used' strategy: whenever a page is accessed the counter increases its value by 1 and is stored in the page's last access attribute
uint32_t lru_counter = 0;

uint8_t hd_mem[4194304]; //slower memory (2^22 -> 4194304 bytes)
uint8_t ra_mem[65536];	//Arbeitspeicher (2^16 -> 65536 bytes) (65536/4096 = 16 Seiten von 4KB)

struct seitentabellen_zeile {
	uint8_t present_bit;//checks if it's in Arbeitsspeicher
	uint8_t dirty_bit;//checks if changes were made
	int8_t page_frame;//4 MSBs de phys. address
	uint32_t last_access;//counter for LRU strategy
}seitentabelle[1024];//Seitentabelle: 1024 Seitentabellen Zeilen

uint16_t get_seiten_nr(uint32_t virt_address) {
	/**
	 *
	 */
	return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
	/**
	 * Wandelt eine virtuelle Adresse in eine physikalische Adresse um.
	 * Der Rückgabewert ist die physikalische 16 Bit Adresse.
	 */

	//left-shift 12 positions the page frame of the page in virtual adr. & concatenate the first 12 bits of the virt. adr.
	uint32_t phys_address = (seitentabelle[get_seiten_nr(virt_address)].page_frame << 12) | (virt_address & 0xFFF);
	
	//the result should be a phys. adr. of 16 bits (by using & with 0xFFFF (1111 1111 1111 1111))
    return (uint16_t)(phys_address & 0xFFFF);
}

int8_t check_present(uint32_t virt_address) {
	/**
	 * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
	 */
	return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
	/**
	 * Wenn der Speicher voll ist, gibt die Funktion 1 zurück;
	 */

	//checks if all 16 pages of Arbeitsspeicher are in use
    if(ram_page_number>=16)
		return 1;
	else return 0;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) { // alte addresse! nicht die neue!
	/**
	 * Schreibt eine Seite zurück auf die HD
	 */

	//iterates over 4KB of data (4096 iterations of 1 byte each)
	for (uint16_t counter = 0; counter < 4096; counter++) {
		//and stores 1 page of 4KB from Arbeitsspeicher to HD (with their respective sarting positions):
		hd_mem[(seitennummer << 12) + counter] = ra_mem[(seitentabelle[seitennummer].page_frame << 12) + counter];
	}
	seitentabelle[seitennummer].dirty_bit = 0;//set dirty bit to 0: there were no changes in Arbeitspeicher (in relationship to HD)
	return 0;  
}

uint16_t swap_page(uint32_t virt_address) {
	/**
	 * Das ist die Funktion zur Auslagerung einer Seite.
	 * Wenn das "Dirty_Bit" der Seite in der Seitentabelle gesetzt ist,
	 * muss die Seite zurück in den hd_mem geschrieben werden.
	 * Welche Rückschreibstrategie (LRU) Sie implementieren möchten, ist Ihnen überlassen.
	 */

	uint16_t lru_page_index = 0;//index of least recently used page
	time_t oldest_access = time(NULL);//sets (at first) oldest access as current time in seconds (upper bound for later comparison)

	//find the least recently used page by...
	for(int i = 0; i < 1024; i++) {
		//...checking if it's in the Arbeitsspeicher & always keeping "oldest access" minimal
    	if (seitentabelle[i].present_bit && seitentabelle[i].last_access < oldest_access) {
			oldest_access = seitentabelle[i].last_access;
        	lru_page_index = i;
    	}
	}
	//at the end of the for-loop, the least recently used page's index (minimal value of "last access" attribute of all 1024 pages) is stored in the variable lru_page_index

	//when dirty bit of said page is set to 1, write that page back to HD before...
	if (seitentabelle[lru_page_index].dirty_bit == 1)
		write_page_to_hd(lru_page_index,virt_address);

	seitentabelle[lru_page_index].present_bit = 0;//... removing it from Arbeitsspeicher
	return seitentabelle[lru_page_index].page_frame;//returns page frame to be overwritten
}

int8_t get_page_from_hd(uint32_t virt_address) {
	/**
	 * Lädt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
	 * Erstellt einen Seitentabelleneintrag.
	 * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
	 */

	if(is_mem_full())//if Arbeitsspeicher is full...
		seitentabelle[get_seiten_nr(virt_address)].page_frame = swap_page(virt_address);//...swap page and set page frame of new table entry

	else {//if Arbeitsspeicher still has available free pages...
		seitentabelle[get_seiten_nr(virt_address)].page_frame = ram_page_number;//...use the next free page to set page frame of new table entry
		ram_page_number++;//... and update global counter of ram pages (+1) (reminder: max. amount of possible ram pages is 16 (from 0 to 15))
	}

	//save data from HD to Arbeitsspeicher:
	for(uint16_t counter = 0; counter < 4096;counter++) { //iterate over a page (4KB = 4096 bytes)
		//store value of HD to Arbeitsspeicher (with their respective addresses and starting points)
		ra_mem[(seitentabelle[get_seiten_nr(virt_address)].page_frame << 12) + counter] = hd_mem[(get_seiten_nr(virt_address) << 12) + counter];
	}

	//set attributes of newly created table entry:
	seitentabelle[get_seiten_nr(virt_address)].present_bit = 1; //page is now in Arbeitsspeicher
	seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;	//and has had no changes since loaded from HD
	return 0;
}

uint8_t get_data(uint32_t virt_address) {
	/**
	 * Gibt ein Byte aus dem Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 * Die definition dieser Funktion darf nicht geaendert werden. Namen, Parameter und Rückgabewert muss beibehalten werden!
	 */

	lru_counter++; //update lru counter
	seitentabelle[get_seiten_nr(virt_address)].last_access = lru_counter; //update last access of page being accessed

	//if page is not in Arbeitsspeicher...
	if (!check_present(virt_address))
        get_page_from_hd(virt_address);//... get page from HD and copy it in Arbeitsspeicher

	uint16_t phys_address = virt_2_ram_address(virt_address);//access Arbeitsspeicher with phys. address
    return ra_mem[phys_address];//return byte from Arbeitsspeicher's phys. address
}

void set_data(uint32_t virt_address, uint8_t value) {
	/**
	 * Schreibt ein Byte in den Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 */

	lru_counter++; //update lru counter
	seitentabelle[get_seiten_nr(virt_address)].last_access = lru_counter; //update last access of page being accessed

	//if page is not in Arbeitsspeicher...
    if (!check_present(virt_address))
        get_page_from_hd(virt_address);//... get page from HD and copy it in Arbeitsspeicher

    uint16_t phys_address = virt_2_ram_address(virt_address); //translate virtual adr. to phys. adr.
	uint16_t page_number = get_seiten_nr(virt_address); //get page number from virt. adr.
	seitentabelle[page_number].dirty_bit = 1; //set dirt bit to 1 because changes were made
	ra_mem[phys_address] = value; //update value of Arbeitsspeicher in phys. adr.
}

int main(void) {
	puts("test driver_");
	static uint8_t hd_mem_expected[4194304];
	srand(1);
	fflush(stdout);

	for(int i = 0; i < 4194304; i++) {
		//printf("%d\n",i);
		uint8_t val = (uint8_t)rand();
		hd_mem[i] = val;
		hd_mem_expected[i] = val;
	}

	for (uint32_t i = 0; i < 1024;i++) {
//		printf("%d\n",i);
		seitentabelle[i].dirty_bit = 0;
		seitentabelle[i].page_frame = -1;
		seitentabelle[i].present_bit = 0;
	}


	uint32_t zufallsadresse = 4192425;
	uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

	if(hd_mem[zufallsadresse] != value) {
		printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
	}

	value = get_data(zufallsadresse);

	if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);

	}

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


	srand(3);

	for (uint32_t i = 0; i <= 1000;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(1);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}


	srand(3);

	for (uint32_t i = 0; i <= 100;i++) {
		uint32_t zufallsadresse = rand() % 4095 *7;
		uint8_t value = (uint8_t)zufallsadresse >> 1;
		set_data(zufallsadresse, value);
		hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
	}



	srand(4);
	for (uint32_t i = 0; i <= 16;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value) {
//			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}

			exit(2);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}

	srand(3);
	for (uint32_t i = 0; i <= 2500;i++) {
		uint32_t zufallsadresse = rand() % (4095 *5);//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem_expected[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(3);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
		fflush(stdout);
	}

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}