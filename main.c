#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

FILE *ipg;

// Cuenten con este codigo monolitico en una funcion
// main como punto de partida.
// Idealmente, el codigo del programa deberia estar
// adecuadamente modularizado en distintas funciones,
// e incluso en archivos separados, con dependencias
// distribuidas en headers.

//Variables Globales.
int bankId;
//Pipes.
int outwPipes[50][2];//salida de banco principal.
int inwPipes[50][2];//entrada al banco principal.

char branches[50][5];//Sucursales.
char branchesac[50][7];//cantidad de cuentas en cada sucursal.
char transc[20000][40];//transacciones generadas en cada sucursal.
char transcfail[20000][70];
int tr = 0;
int trfail = 0; //cantidad de errores hasta el momento.
int forks = 0; //contador de fork actual.
int counter = 0; //contador general de sucursales andando.
int terminated = 0; //Indicador para terminar thread.
char readbuffer[100]; // buffer para lectura desde pipe de salida.
char readbuffer2[100]; // buffer para lectura desde pipe de entrada.
int *cash;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//metodos con lock para la plata de las cuentas.
void addcash(int cual, int cuanto){
	pthread_mutex_lock(&mutex);
	cash[cual]+=cuanto;
	pthread_mutex_unlock(&mutex);
}
void subcash(int cual, int cuanto){
	pthread_mutex_lock(&mutex);
	cash[cual]-=cuanto;
	pthread_mutex_unlock(&mutex);
}
void adderror(char *new){
	pthread_mutex_lock(&mutex);
	strcpy(transcfail[trfail],new);
	trfail+=1;
	pthread_mutex_unlock(&mutex);
}
void addtransc(char *new){
	pthread_mutex_lock(&mutex);
	strcpy(transc[tr],new);
	tr+=1;
	pthread_mutex_unlock(&mutex);
}

//Thread de banco principal.
void *parent(void *arg){
	while(terminated == 0){
		for(int j = 0; j < forks; j++){
			if (atoi(branches[j])>-1){
				//Lector de transacciones.
				int five = 0;
				int bytes = read(inwPipes[j][0], readbuffer2, sizeof(readbuffer2));
				//printf("sucursal %s dice: '%s' \n",branches[j], readbuffer2);
				for(int i = 0; i < strlen(readbuffer2); i++){
					if(readbuffer2[i]==','){
						five+=1;
					}
				}
				if(five==5){
					char  copiaoriginal[50];
					strcpy(copiaoriginal,readbuffer2);
					char* tipo;
					char* sucursalorigen;
					char* cuentaorigen;
					char* sucursaldestino;
					char* cuentadestino;
					char* monto;
					int existe = 0;
					tipo = strtok(copiaoriginal, ","); 
					sucursalorigen = strtok(NULL, ",");
					cuentaorigen = strtok(NULL, ",");
					sucursaldestino=strtok(NULL, ",");
					cuentadestino=strtok(NULL,",");
					monto = strtok(NULL,",");
					//redireccion de operaciones.
					for (int k = 0; k<forks;k++){
						if((strcmp(branches[k],sucursaldestino)==0) && (atoi(branchesac[k])>= atoi(cuentadestino))){
							existe=1;
							if (strcmp(tipo,"DP")==0){
								char asd[35];
								sprintf(asd,"%s,%s,%s","DP",cuentadestino,monto);
								//printf("asd: %s sucursal %s \n",asd,branches[k]);
								write(outwPipes[k][1], asd, (strlen(asd)+1));
							
							}
							else{
								char  cp[40];
								strcpy(cp,readbuffer2);
								//printf("cp: %s sucursal %s \n",cp,branches[k]);
								write(outwPipes[k][1],cp,(strlen(cp)+1));
							}
						
						}
					}
					//error de cuenta o sucursal cerrada.
					if(existe==0){
						char  co[40];
						strcpy(co,readbuffer2);
						char mens [40]="FAIL,";
						strcat(mens,co);
						write(outwPipes[j][1], mens, (strlen(mens)+1));

					}
					(void)cuentaorigen;
					(void)sucursalorigen;
				}
				(void)bytes;
			}
			strcpy(readbuffer2,"");
		}
	}
	return NULL;
}

//Thread de sucursales.
void *child(void *arg){

	int t;
	srand(time(NULL));
	
	while(terminated == 0){
		//Generacion de transacciones.
		char msg[50]="";
		char guardartransc[30]="";
		t=100000;
		t+=rand()%400000;
		usleep(t);
		int tipo = rand()%2;
		if (tipo==1){
			strcpy(msg,"DP,");
			strcpy(guardartransc,"DP,");
		}else{
			strcpy(msg,"RT,");
			strcpy(guardartransc,"RT,");
		}
		strcat(msg,branches[forks]);
		strcat(msg,",");
		strcat(guardartransc,branches[forks]);
		strcat(guardartransc,",");
		char sourcec[7];
		int sourcei = rand()%atoi(branchesac[forks]);
		sprintf(sourcec,"%d",sourcei);
		strcat(msg,sourcec);
		strcat(msg,",");
		strcat(guardartransc,sourcec);
		strcat(guardartransc,",");
		int destiny = rand()%counter;
		while(atoi(branches[destiny])<0){
			destiny = rand()%counter;
		}
		char branch[4]="";
		strcat(branch,branches[destiny]);
		strcat(msg,branch);
		strcat(msg,",");
		char account[10]="";
		int ac = rand()%10000;
		
		sprintf(account,"%d",ac);
		strcat(msg,account);
		strcat(msg,",");
		strcat(guardartransc,account);
		
		char out[10];
		int money = rand()%500000000;
		sprintf(out,"%d",money);
		strcat(msg,out);
		if (tipo ==1){
			//Si es un deposito, se asegura de tener los fondos, o anotar el error.
			if (cash[sourcei]>money){
				subcash(sourcei,money);
				write(inwPipes[forks][1], msg, strlen(msg)+1);
			}else{ 	//error
				char failfinal [80]="";
				strcat(failfinal,"1,");
				strcat(failfinal,sourcec);
				strcat(failfinal,",");
				char montoactual [10];
				sprintf(montoactual,"%d",cash[sourcei]);
				char montoretiro[10];
				sprintf(montoretiro,"%d",money);
				strcat(failfinal,montoactual);
				strcat(failfinal,",");
				strcat(failfinal,montoretiro);
				
				adderror(failfinal);
			
			}

		}
		//Si es retiro.
		else{
			write(inwPipes[forks][1], msg, strlen(msg)+1);
			
		}
		addtransc(guardartransc);
		usleep(t);
	}
	usleep(1500000);
	char bye[] = " ";
	write(inwPipes[forks][1], bye, strlen(bye)+1);
	return NULL;
}

int main(int argc, char** argv) {
	
	size_t bufsize = 512;
	char* commandBuf = malloc(sizeof(char)*bufsize);
	// Para guardar descriptores de pipe
	// el elemento 0 es para lectura
	// y el elemento 1 es para escritura.
	for (int i = 0; i<50;i++){
		pipe(outwPipes[i]);
		pipe(inwPipes[i]);
	}
	
	bankId = getpid() % 1000;
	printf("Bienvenido a Banco '%d'\n", bankId);
	//Creacion de thread del banco.
	pthread_t th;
	pthread_create(&th, NULL,parent,NULL);
	while (true) {
		usleep(200000);
		printf(">>");
		getline(&commandBuf, &bufsize, stdin);
	
		// Manera de eliminar el \n leido por getline
		commandBuf[strlen(commandBuf)-1] = '\0';
		printf("Comando ingresado: '%s'\n", commandBuf);

		if (!strncmp("quit", commandBuf, strlen("quit"))) {
			break;
		}
		//comando para ver sucursales activas.
		else if (!strncmp("list", commandBuf, strlen("list"))) {

			printf("| PID | N° cuentas | \n");
			for (int j = 0; j<forks;j++){
				if (atoi(branches[j])>-1){
					printf("| %3s |%12s| \n",branches[j],branchesac[j]);
				}
			}	
			continue;
		}
		//comando para iniciar nueva sucursal.
		else if (!strncmp("init", commandBuf, strlen("init"))) {
			// OJO: Llamar a fork dentro de un ciclo
			// es potencialmente peligroso, dado que accidentalmente
			// pueden iniciarse procesos sin control.
			// Buscar en Google "fork bomb"
			char * numerocuentas;
			char * numerothreads;
			//int largo = strlen(commandBuf);
			numerocuentas = strtok(commandBuf, " ");
			char n[7] = "1000";
			char thds[2] = "1";
			numerocuentas=strtok(NULL," ");
			if (numerocuentas && atoi(numerocuentas)>0 && atoi(numerocuentas)<=10000){	
				strcpy(n,numerocuentas);
			}
			numerothreads=strtok(NULL," ");
			if (numerothreads && atoi(numerothreads)>0 && atoi(numerothreads)<9){	
				strcpy(thds,numerothreads);
			}
			pid_t sucid = fork();	
			if (sucid > 0) {
				
				printf("Sucursal creada con ID '%d'\n", sucid);
				int sucId = sucid % 1000;
				sprintf(branches[forks],"%d",sucId);
				strcpy(branchesac[forks], n);
				for (int i = 0; i < forks; i++){
					if (atoi(branches[i])>-1){	
						char NS[13];
						sprintf(NS,"NS %d-%s",sucId,n);
						write(outwPipes[i][1], NS,13);
					}
				}
				forks +=1;
				counter +=1;
				continue;
			}
			// Proceso de sucursal
			else if (!sucid) {
				int sucId = getpid() % 1000;
				sprintf(branches[forks],"%d",sucId);
				
				strcpy(branchesac[forks], n);
				counter +=1;
				cash= malloc(atoi(n) * sizeof *cash);
				srand(time(NULL));
				for (int i=0; i< atoi(n); i++){
					cash[i]=1000;
					cash[i]+=rand()%499999000;
				}
				pthread_t tc[atoi(thds)];
				for (int i = 0; i < atoi(thds); i++){
					pthread_create(&tc[i], NULL,child,NULL);
				}
				printf("Hola, soy la sucursal '%d' y tengo '%s' cuentas \n", sucId, n);
				while (true) {
					// 100 milisegundos...
					int bytes = read(outwPipes[forks][0], readbuffer, sizeof(readbuffer));
					//printf("Soy la sucursal '%d' y me llego mensaje '%s' de '%d' bytes.\n", sucId,readbuffer, bytes);

					// Usar usleep para dormir una cantidad de microsegundos
					//usleep(1000000);

					// Cerrar lado de lectura del pipe
					//close(bankPipe[0]);

					// Para terminar, el proceso hijo debe llamar a _exit,
					// debido a razones documentadas aqui:
					// https://goo.gl/Yxyuxb
					char *aux;
					char *nada;
					char name[20]="";
					//manejo de error en momento de que maten una sucursal a la que se le estaba enviando una transaccion.
					if (!strncmp("FAIL", readbuffer, strlen("FAIL"))){
						char * tipo;
						char * cuentao;
						char * cuentad;
						char * monto;
						nada=strtok(readbuffer, ",");//nada
						tipo=strtok(NULL,",");
						nada=strtok(NULL,",");//so
						cuentao=strtok(NULL,",");//co
						nada=strtok(NULL,",");//sd
						cuentad=strtok(NULL,",");//cd
						monto =strtok(NULL,",");//monto
						if (strncmp("DP", tipo,2)==0){
							addcash(atoi(cuentao),atoi(monto));// le devuelvo la plata
						}
						char fail[6];
						strcpy(fail,"2,");
						strcat(fail,cuentad);
						adderror(fail);
		
					}
					//manejo de depositos hacia esta sucursal.
					else if (!strncmp("DP", readbuffer, strlen("DP"))){
						
						//depositar
						char * cuenta;
						char * monto;
						
						nada=strtok(readbuffer, ",");
						cuenta=strtok(NULL, ",");
						monto=strtok(NULL,",");
						addcash(atoi(cuenta),atoi(monto));

						
					}
					//manejo de retiros en esta sucursal.
					else if (!strncmp("RT", readbuffer, strlen("RT"))){
						//char copia[34];
						char dep[34];
						char * cuenta;
						char * monto;
						char montop[7]="";
						char * so;
						char * co;
						char * sd;
						char error[20]="";
						char newtransac[30]="";

						
						//strcpy(copia,readbuffer);//lo hago aca para ver si funca
						nada=strtok(readbuffer, ",");//tipo
						so=strtok(NULL,",");//so
						co=strtok(NULL,",");//co
						sd=strtok(NULL,",");//sd
						cuenta=strtok(NULL,",");//cd
						sprintf(montop,"%d",cash[atoi(cuenta)]);
						monto =strtok(NULL,",");//monto
						if (cash[atoi(cuenta)]>=atoi(monto)){
							subcash(atoi(cuenta),atoi(monto));
							strcpy(dep,"DP,");
							strcat(dep,sd);
							strcat(dep,",");
							strcat(dep,cuenta);
							strcat(dep,",");
							strcat(dep,so);
							strcat(dep,",");
							strcat(dep,co);
							strcat(dep,",");
							strcat(dep,monto);
							
							strcpy(newtransac,"DP,");
							strcat(newtransac,so);
							strcat(newtransac,",");
							strcat(newtransac,co);
							strcat(newtransac,",");
							strcat(newtransac,cuenta);
							addtransc(newtransac);
							for(int i = 0; i<counter;i++){
								if(strcmp(branches[i],so)==0){
									write(inwPipes[forks][1], dep, strlen(dep)+1);
								}
							}
						}
						//error en el caso de no tener el dinero para el retiro pedido.
						else{
							strcpy(error,"1,");
							strcat(error,cuenta);
							strcat(error,",");
							strcat(error,montop);
							strcat(error,",");
							strcat(error,monto);
							adderror(error);
						}
						
					}
					//aviso de nueva sucursal abierta.
					else if (!strncmp("NS", readbuffer, strlen("NS"))){
						char id[3]="";
						char cuentas[7]="";
						aux=strtok(readbuffer, " ");
						aux=strtok(NULL," ");			 	
						aux=strtok(aux,"-");
						strcpy(id,aux);
						aux=strtok(NULL," ");
						strcpy(cuentas,aux);				
						strcpy(branches[counter],id);
						strcpy(branchesac[counter],cuentas);
						counter +=1;
					}
					//señal para terminar proceso.
					else if (!strncmp("kill", readbuffer, strlen("kill"))){
						aux=strtok(readbuffer, " ");
						aux=strtok(NULL," ");
						if(strcmp(branches[forks],aux)==0){
							terminated = 1;
							for (int i = 0; i < atoi(thds); i++){
								pthread_join(tc[i],NULL);
							}
							free(cash);		 	
							_exit(EXIT_SUCCESS);
						}
						for (int j = 0; j<counter;j++){
							if(strcmp(branches[j],aux)==0){
								strcpy(branches[j],"-1");
							}
						}
						
					}
					//dump de cuentas.
					else if (!strncmp("dump_accs", readbuffer, strlen("dump_accs"))){
						sprintf(name,"dump_accs_%s.csv",branches[forks]);
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"Numero de cuenta, saldo\n");
						for (int i=0; i<atoi(n); i++){
							fprintf(ipg,"%06d, %d \n", i, cash[i]);
						}
					        
					        if(ipg!=NULL) fclose(ipg);	
					
					}//dump de transacciones con error.
					else if (!strncmp("dump_errs", readbuffer, strlen("dump_errs"))){
						sprintf(name,"dump_errs_%s.csv",branches[forks]);
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"tipo de error, número de cuenta, saldo previo a la transacción, monto que se quiso retirar\n");
						for (int i=0; i<trfail; i++){
							
							fprintf(ipg,"%s\n", transcfail[i]);
						}
						if(ipg!=NULL) fclose(ipg);
					
					}
					//dump de transacciones generadas.
					else if (!strncmp("dump", readbuffer, strlen("dump"))){
						sprintf(name,"dump_%s.csv",branches[forks]);
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"tipo de transacción, medio de origen, cuenta de origen, cuenta de destino\n");
						for (int i=0; i<tr; i++){
							fprintf(ipg,"%s \n", transc[i]);
						}
						if(ipg!=NULL) fclose(ipg);
					
					}
					(void)nada;
					(void)bytes;	
					
				}
			}
			// error
			else {
				fprintf(stderr, "Error al crear proceso de sucursal!\n");
				return (EXIT_FAILURE);
			}
		}
		//comando para eliminar determinada sucursal.
		else if (!strncmp("kill", commandBuf, strlen("kill"))) {
			char *id;
			char aux[15]="";
			if (strlen(commandBuf)<5){
				printf("ERROR Se debe ingresar un id de sucursal\n");
				continue;
			}
			strcpy(aux,commandBuf);
			id=strtok(aux, " ");
			id=strtok(NULL," ");
			for (int j = 0; j<forks;j++){
				write(outwPipes[j][1], commandBuf, (strlen(commandBuf)+1));
				if(strcmp(branches[j],id)==0){
					strcpy(branches[j],"-1");
				}
			}
			continue;
		}
		//comando para generar determinado dump.
		else if (!strncmp("dump_accs", commandBuf, strlen("dump_accs"))) {
			char *id;
			if (strlen(commandBuf)<10){
				printf("ERROR Se debe ingresar un id de sucursal\n");
				continue;
			}
			id=strtok(commandBuf, " ");
			id=strtok(NULL," ");
			if(id!=NULL){
				for (int j = 0; j<forks;j++){
					if(strcmp(branches[j],id)==0){
						write(outwPipes[j][1], commandBuf, (strlen(commandBuf)+1));
					}
				}
			}
			continue;

		}else if (!strncmp("dump", commandBuf, strlen("dump"))) {
			char *id;
			if (strlen(commandBuf)<5){
				printf("ERROR Se debe ingresar un id de sucursal\n");
				continue;
			}
			id=strtok(commandBuf, " ");
			id=strtok(NULL," ");
			if(id!=NULL){
				for (int j = 0; j<forks;j++){
					if(strcmp(branches[j],id)==0){
						write(outwPipes[j][1], commandBuf, (strlen(commandBuf)+1));
					}
				}
			}
			continue;

		}else if (!strncmp("dump_errs", commandBuf, strlen("dump_errs"))) {
			char *id;
			if (strlen(commandBuf)<10){
				printf("ERROR Se debe ingresar un id de sucursal\n");
				continue;
			}
			id=strtok(commandBuf, " ");
			id=strtok(NULL," ");
			if(id!=NULL){
				for (int j = 0; j<forks;j++){
					if(strcmp(branches[j],id)==0){
						write(outwPipes[j][1], commandBuf, (strlen(commandBuf)+1));
					}
				}
			}
			continue;

		}
			
		else {
			fprintf(stderr, "Comando no reconocido.\n");
		}
	}
	//cuando se termina el banco principal, se envia mensajes para terminar todas las sucursales aun vivas.
	for (int i = 0; i < forks; i++){
		if (atoi(branches[i])>-1){	
			char msg[8] = "kill ";
			strcat(msg,branches[i]);
			write(outwPipes[i][1], msg,strlen(msg)+1);
		}
	}
	
	//señal para terminar thread concurrente.
	terminated = 1;
	free(cash);
  	printf("Terminando ejecucion limpiamente...\n");
	wait(NULL);
  	return(EXIT_SUCCESS);
}
