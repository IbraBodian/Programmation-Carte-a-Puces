// fichier "io.c"
// lecture et écriture d'octets en t=0
// prototype des fonctions définies dans ce fichier :
// uint8_t recbytet0();		// reçoit un octet t=0
// void sendbytet0(uint8_t);	// émet un octet t=0


#include <inttypes.h>
#include <avr/io.h>


// un bit tous les 372 clocks -- 1 etu = 372 clock de l'entrée clock externe
// 8 * 46.5
// 8 * 46 =  368
// Un etu (elementary time unit) fait théoriquement 372 coups d'horloge externe, soit
// environ 46 itérations du compteur CNT2 cadencé à CK/8


#define IOPIN	4
/*
// attente synchrone avec CK EXT
static void attendre(uint8_t d)
{
	TCNT2=0;
	do{} while(TCNT2<=d);
}
*/
// envoi d'un bit sur le lien série
static void sendbit(uint8_t b)
{
	uint8_t outB;
	// calcule la valeur à sortir
	outB=0;
	if ((b&1)!=0) outB=(1<<IOPIN);
	// attend la fin de l'envoi de l'octet précédent
	do{} while (TCNT2<=45);
	// relance le compteur
	TCNT2=0; 
	// positionne la sortie
	PORTB=outB;
}

// envoi d'un octet sur le lien série
void sendbytet0(uint8_t b)
{
	uint8_t i;	// compteur
	uint8_t p;	// parité
	uint8_t tccr2b_save;
	
	tccr2b_save=TCCR2B;
	TCCR2B=2;	// lance le compteur sur CK/8
	TCNT2=40;	// initialise TCNT2 pour attente lors du premier envoi
reenvoyer:
	DDRB|=1<<IOPIN;	// IO en sortie
	PORTB|=1<<IOPIN;
	// bit start
	sendbit(0);
	p=0;
	// chaque bit de l'octet
	for (i=0;i<8;i++)
	{
		sendbit(b);
		p^=b;
		b>>=1;
	}
	// bit de parité
	sendbit(p);
	// bit stop
	sendbit(1);
	do{} while (TCNT2<=45); // attendre fin du bit stop 
	// lecture du code d'erreur

	// commuter en mode entrée
	DDRB&=~(1<<IOPIN);
	PORTB&=~(1<<IOPIN);

	// attendre encore 1 etu
	do; while (TCNT2<=91);
	// lire le signal d'erreur
	if ((PINB&(1<<IOPIN))==0)
	{ 	// si on lit 0
		do {} while ((PINB&(1<<IOPIN))==0);	// attendre la fin du signal d'erreur
		TCNT2=22;	// positionner le compteur pour attendre encore 1/etu avant envoi
		goto reenvoyer;
	}
	TCCR2B=tccr2b_save;	// restauration de l'ancienne valeur de TCCR2B
}


// lecture d'un bit
static uint8_t getbit()
{
	uint8_t b;
	// attendre la fin du bit précédent
	do{}while (TCNT2<=45);
	TCNT2=0;	// réinitialise le compteur
	// vote majoritaire sur lecture à trois instants
	do{}while (TCNT2<15);
	b=(PINB&(1<<4));
	do{}while (TCNT2<=23);
	b+=(PINB&(1<<4));
	do{}while (TCNT2<=30);
	b+=(PINB&(1<<4));
	return (b>>5)&1;
}

uint8_t recbytet0()
{
	uint8_t i;	// compteur
	uint8_t r;	// résultat
	uint8_t p;	// parité
	uint8_t b;	// bit reçu
	
	TCCR2B=2;	// démarre CNT2 sur CKEXT/8
relire:
	DDRB&=~(1<<4);		// mode entrée sur pb4
	PORTB|=(1<<4);
	r=0;
	// attendre le bit start
	do{}while (PINB&(1<<4));
	do{}while (PINB&(1<<4));	// anti rebond
	TCNT2=0;
	p=0;
	for (i=0;i<8;i++)
	{
		b=getbit();
		r+=(b<<i);
		p+=b;
	}
	p=(getbit()^p)&1;	// p contient 1 si erreur de parité
	// attendre la fin du bit de parité + 1 etu 
	do{} while (TCNT2<=91);
	// si erreur de parité, demander une réémission en mettant la ligne à 0 pendant environ 1.5etua
	if (p)
	{
		DDRB|=(1<<IOPIN);	// sortie
		PORTB&=~(1<<IOPIN);	// signal 0
		do; while (TCNT2<=151);
//		attendre(69);		// pendant 1.5 etu
		goto relire;
	}
	else
	{
		// sinon, attendre  1 etu du bit stop
		do; while (TCNT2<=137);
//		attendre(45);
	}
	TCCR2B=0;	// arrêter le compteur
	return r;
}



