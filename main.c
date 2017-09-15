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
char brancheter[50][2];//cantidad de terminales en cada sucursal.
char transc[100000][40];//transacciones generadas en cada sucursal.
char transcfail[100000][70];//transacciones fallidas por falta de plata o cuenta inexistente.
int tr = 0; //contador de transacciones.
int trfail = 0; //cantidad de errores hasta el momento.
int forks = 0; //contador de fork actual.
int counter = 0; //contador general de sucursales andando.
int terminated = 0; //Indicador para terminar thread.
char readbuffer[40]; // buffer para lectura desde pipe de salida.
char readbuffer2[40]; // buffer para lectura desde pipe de entrada.
int *cash; //Arreglo con la plata de cada cuenta, se hace malloc despues de especificar 
pthread_mutex_t mutex[10000];//Mutex para cada cuenta.
pthread_mutex_t emutex = PTHREAD_MUTEX_INITIALIZER;//Mutex  para agregar errores y hacer dump de estos.
pthread_mutex_t tmutex = PTHREAD_MUTEX_INITIALIZER;//Mutex  para agregar transacciones y hacer dump de estas.
pthread_mutex_t rmutex = PTHREAD_MUTEX_INITIALIZER;//Mutex  para agregar escribir en los pipes.

//Metodos con lock para la plata de las cuentas. Cada cuenta tiene un Mutex propio.
void addcash(int cual, int cuanto){
	pthread_mutex_lock(&mutex[cual]);
	cash[cual]+=cuanto;
	pthread_mutex_unlock(&mutex[cual]);
}
void subcash(int cual, int cuanto){
	pthread_mutex_lock(&mutex[cual]);
	cash[cual]-=cuanto;
	pthread_mutex_unlock(&mutex[cual]);
}
//Metodo para agregar errores.
void adderror(char *new){
	pthread_mutex_lock(&emutex);
	strcpy(transcfail[trfail],new);
	trfail+=1;
	pthread_mutex_unlock(&emutex);
}
//Metodo para agregar transacciones.
void addtransc(char *new){
	pthread_mutex_lock(&tmutex);
	strcpy(transc[tr],new);
	tr+=1;
	pthread_mutex_unlock(&tmutex);
}

//Thread de banco principal.
void *parent(void *arg){
	while(terminated == 0){
		for(int j = 0; j < forks; j++){
			if (atoi(branches[j])>-1){
				//Lector de transacciones.
				for(int l = 0; l < atoi(brancheter[j]) ; l++){
					int five = 0;
					//Hacemos read caracter por caracter del pipe, cada mensaje termina en un '\0'.
					char buff[2];
					buff[1]='\0';
					while(read(inwPipes[j][0], buff, 1)>0){
						if(strcmp(buff,"\0")==0){
							break;
						}
						strcat(readbuffer2,buff);
					}
					for(int i = 0; i < strlen(readbuffer2); i++){
						if(readbuffer2[i]==','){
							five+=1;
						}
					}
					//Se revisa que el mensaje venga con el formato correcto.
					if(five==5 && ( (!strncmp("DP", readbuffer2, strlen("DP"))) || (!strncmp("RT", readbuffer2, strlen("RT")))) ){
						char  copiaoriginal[40];
						strcpy(copiaoriginal,readbuffer2);
						char* tipo;
						char* sucursaldestino;
						char* cuentadestino;
						char* monto;
						int existe = 0;
						tipo = strtok(copiaoriginal, ","); 
						strtok(NULL, ",");
						strtok(NULL, ",");
						sucursaldestino=strtok(NULL, ",");
						cuentadestino=strtok(NULL,",");
						monto = strtok(NULL,",");
						//redireccion de operaciones.
						for (int k = 0; k<forks;k++){
							if((strcmp(branches[k],sucursaldestino)==0) && (atoi(branchesac[k])>= atoi(cuentadestino))){
								//existe es para verificar que la cuenta a la que se quiera enviar la transaccion aun exista.
								existe=1;
								if (strcmp(tipo,"DP")==0){
									char asd[40];
									sprintf(asd,"%s,%s,%s","DP",cuentadestino,monto);
									write(outwPipes[k][1], asd, (strlen(asd)+1));
							
								}
								else{
									char  cp[40];
									strcpy(cp,readbuffer2);
									write(outwPipes[k][1],cp,(strlen(cp)+1));
								}
						
							}
						}
						//error de cuenta o sucursal cerrada. se le envia un error a la sucursal original.
						if(existe==0){
							char  co[40];
							strcpy(co,readbuffer2);
							char mens [40]="FAIL,";
							strcat(mens,co);
							write(outwPipes[j][1], mens, (strlen(mens)+1));
						}
					}
					//Se reinicia el buffer devolviendo sus valores a 0.
					memset(&readbuffer2[0], 0, sizeof(readbuffer2));
				}
			}
		}
	}
	return NULL;
}

//Thread de sucursales.
void *child(void *arg){

	while(terminated == 0){
		//Generacion de transacciones.
		char msg[80]=""; //Texto a enviar.
		char guardartransc[30]=""; //Registro de transaccion a guardar.
		int t = 100000;
		t+=rand()%400000;
		usleep(t);
		//Tipo de transaccion. DP = Deposito, RT = Retiro.
		int tipo = rand()%2;
		if (tipo==1){
			strcpy(msg,"DP,");
			strcpy(guardartransc,"DP,");
		}else{
			strcpy(msg,"RT,");
			strcpy(guardartransc,"RT,");
		}
		//Sucursal origen (donde se genero la transaccion).
		strcat(msg,branches[forks]);
		strcat(msg,",");
		strcat(guardartransc,branches[forks]);
		strcat(guardartransc,",");
		char sourcec[7];
		//Cuenta de Origen a la que se le quiere agregar o quitar plata.
		int sourcei = rand()%atoi(branchesac[forks]);
		sprintf(sourcec,"%d",sourcei);
		strcat(msg,sourcec);
		strcat(msg,",");
		strcat(guardartransc,sourcec);
		strcat(guardartransc,",");
		//Sucursal de Destino a la que se le quiere agregar o quitar plata.
		int destiny = rand()%counter;
		while(atoi(branches[destiny])<0){
			destiny = rand()%counter;
		}
		char branch[4]="";
		strcat(branch,branches[destiny]);
		strcat(msg,branch);
		strcat(msg,",");
		//Cuenta de Destino a la que se le quiere agregar o quitar plata. La cuenta es random, el banco matriz revisa si existe la cuenta.
		char account[6]="";
		int ac = rand()%10000;
		
		sprintf(account,"%d",ac);
		strcat(msg,account);
		strcat(msg,",");
		strcat(guardartransc,account);
		//Cantidad de dinero a depositar o retirar.
		char out[10];
		int money = rand()%500000000;
		sprintf(out,"%d",money);
		strcat(msg,out);
		msg[strlen(msg)]='\0';
		if (tipo ==1){
			//Si es un deposito, se asegura de tener los fondos, o anotar el error.
			if (cash[sourcei]>money){
				subcash(sourcei,money);
				pthread_mutex_lock(&rmutex);
				write(inwPipes[forks][1], msg, strlen(msg)+1);
				pthread_mutex_unlock(&rmutex);
				//Se guarda la transaccion generada.
				addtransc(guardartransc);
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
			pthread_mutex_lock(&rmutex);
			write(inwPipes[forks][1], msg, strlen(msg)+1);
			pthread_mutex_unlock(&rmutex);
			//Se guarda la transaccion generada.
			addtransc(guardartransc);	
		}
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
	
		//Manera de eliminar el \n leido por getline
		commandBuf[strlen(commandBuf)-1] = '\0';
		printf("Comando ingresado: '%s'\n", commandBuf);

		if (!strncmp("quit", commandBuf, strlen("quit"))) {
			break;
		}
		//Comando para ver sucursales activas.
		else if (!strncmp("list", commandBuf, strlen("list"))) {

			printf("| PID | N° cuentas | Terminales | \n");
			for (int j = 0; j<forks;j++){
				if (atoi(branches[j])>-1){
					printf("| %3s |%12s| %11s| \n",branches[j],branchesac[j],brancheter[j]);
				}
			}	
			continue;
		}
		//Comando para iniciar nueva sucursal.
		else if (!strncmp("init", commandBuf, strlen("init"))) {
			// OJO: Llamar a fork dentro de un ciclo
			// es potencialmente peligroso, dado que accidentalmente
			// pueden iniciarse procesos sin control.
			// Buscar en Google "fork bomb"
			char * numerocuentas;
			char * numerothreads;
			//int largo = strlen(commandBuf);
			numerocuentas = strtok(commandBuf, " ");
			char n[7] = "1000"; //Cantidad default de cuentas.
			char thds[2] = "1"; //Cantidad default de threads.
			numerocuentas=strtok(NULL," ");
			//Se revisa si se introdujo un numero aceptable de cuentas, 10000 como maximo.
			if (numerocuentas && atoi(numerocuentas)>0 && atoi(numerocuentas)<=10000){	
				strcpy(n,numerocuentas);
			}
			numerothreads=strtok(NULL," ");
			//Se revisa si se introdujo un numero aceptable de threads, 8 como maximo.
			if (numerothreads && atoi(numerothreads)>0 && atoi(numerothreads)<9){	
				strcpy(thds,numerothreads);
			}
			//Fork.
			pid_t sucid = fork();	
			if (sucid > 0) {
				
				printf("Sucursal creada con ID '%d'\n", sucid);
				int sucId = sucid % 1000;
				sprintf(branches[forks],"%d",sucId);
				strcpy(branchesac[forks], n);
				strcpy(brancheter[forks], thds);
				//Aviso a cada sucursal que se ha creado una nueva sucursal.
				for (int i = 0; i < forks; i++){
					if (atoi(branches[i])>-1){	
						char NS[7];
						sprintf(NS,"NS %d",sucId);
						write(outwPipes[i][1], NS,7);
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
				//Se hace malloc con la cantidad de cuentas especificada en el init.
				cash= malloc(atoi(n) * sizeof *cash);
				srand(time(NULL));
				for (int i=0; i< atoi(n); i++){
					cash[i]=1000;
					cash[i]+=rand()%499999000;
					pthread_mutex_init(&mutex[i],NULL);
				}
				//Se inicia la cantidad de threads especificados en el init.
				pthread_t tc[8];
				for (int i = 0; i < atoi(thds); i++){
					pthread_create(&tc[i], NULL,child,NULL);
				}
				printf("Hola, soy la sucursal '%d' y tengo '%s' cuentas \n", sucId, n);
				while (true) {
					//Lector de transacciones, desde banco matriz hacia sucursales. Se lee caracter por caracter.
					char buff[2];
					buff[1]='\0';
					while(read(outwPipes[forks][0], buff, 1)>0){
						if(strcmp(buff,"\0")==0){
							break;
						}
						strcat(readbuffer,buff);
					}
					char *aux;
					char name[20]="";
					//manejo de error en momento de que maten una sucursal a la que se le estaba enviando una transaccion o que la cuenta de destino 						//no existiera. Este es el error tipo 2.
					if (!strncmp("FAIL", readbuffer, strlen("FAIL"))){
						char * tipo;
						char * cuentao;
						char * cuentad;
						char * monto;
						strtok(readbuffer, ",");//nada
						tipo=strtok(NULL,",");
						strtok(NULL,",");//so
						cuentao=strtok(NULL,",");//co
						strtok(NULL,",");//sd
						cuentad=strtok(NULL,",");//cd
						monto =strtok(NULL,",");//monto
						if (strncmp("DP", tipo,2)==0){
							addcash(atoi(cuentao),atoi(monto));// le devuelvo la plata
						}
						char fail[6];
						strcpy(fail,"2,");
						strcat(fail,cuentad);
						//Se agrega el error.
						adderror(fail);
		
					}
					//Manejo de depositos hacia esta sucursal. Se encarga de agregar la plata a la cuenta de destino.
					else if (!strncmp("DP", readbuffer, strlen("DP"))){
						
						//depositar
						char * cuenta;
						char * monto;
						
						strtok(readbuffer, ",");
						cuenta=strtok(NULL, ",");
						monto=strtok(NULL,",");
						addcash(atoi(cuenta),atoi(monto));

						
					}
					//Manejo de retiros en esta sucursal. Se encarga de generar deposito de vuelta a sucursal de origen. En el caso de no tener la 						//plata necesaria, se anota como error de tipo 1.
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

						strtok(readbuffer, ",");//tipo
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
					//aviso de nueva sucursal abierta. esto es para el momento de generar transacciones.
					else if (!strncmp("NS", readbuffer, strlen("NS"))){
						char id[3]="";
						aux=strtok(readbuffer, " ");
						aux=strtok(NULL," ");
						strcpy(id,aux);			
						strcpy(branches[counter],id);
						counter +=1;
					}
					//señal para terminar proceso.
					else if (!strncmp("kill", readbuffer, strlen("kill"))){
						aux=strtok(readbuffer, " ");
						aux=strtok(NULL," ");
						//Si la sucursal a cerrar es esta misma.
						if(strcmp(branches[forks],aux)==0){
							//Se le señala al thread que deberia dejar de generar transacciones y terminar.
							terminated = 1;
							//Se le hace Join a cada thread. Espera que todos los threads terminen antes de terminar el proceso.
							for (int i = 0; i < atoi(thds); i++){
								pthread_join(tc[i],NULL);
							}
							//Se libera el malloc de commandBuf y cash.
							free(cash);	
							free(commandBuf);
							_exit(EXIT_SUCCESS);
						}
						//Si se quiere cerrar otra sucursal, se elimina de sucursales activas.
						for (int j = 0; j<counter;j++){
							if(strcmp(branches[j],aux)==0){
								strcpy(branches[j],"-1");
							}
						}
						
					}
					//dump de cuentas.
					else if (!strncmp("dump_accs", readbuffer, strlen("dump_accs"))){
						//Se le hace lock a todos los mutex de las cuentas, para que no se puedan editar mientras se hace el dump.
						for (int i=0; i< atoi(n); i++){
							pthread_mutex_lock(&mutex[i]);
						}
						//Se le pone el nombre al archivo con el id de la sucursal.
						sprintf(name,"dump_accs_%s.csv",branches[forks]);	
						//Se crea el archivo.
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"Numero de cuenta, saldo\n");
						//Se escribe transaccion una por una en el archivo.
						for (int i=0; i<atoi(n); i++){
							fprintf(ipg,"%06d, %d \n", i, cash[i]);
						}
					        //Se cierra el archivo
					        if(ipg!=NULL) fclose(ipg);
						//Se le hace unlock a todos los mutex de las cuentas.
						for (int i=0; i< atoi(n); i++){
							pthread_mutex_unlock(&mutex[i]);
						}	
					
					}//dump de transacciones con error.
					else if (!strncmp("dump_errs", readbuffer, strlen("dump_errs"))){
						//Se le hace lock al mutex de los errores, para que no se puedan agregar errores mientras se hace el dump.
						pthread_mutex_lock(&emutex);
						//Se le pone el nombre al archivo con el id de la sucursal.
						sprintf(name,"dump_errs_%s.csv",branches[forks]);
						//Se crea el archivo.
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"tipo de error, número de cuenta, saldo previo a la transacción, monto que se quiso retirar\n");
						//Se escriben los errores en el archivo, uno por uno.
						for (int i=0; i<trfail; i++){
							fprintf(ipg,"%s\n", transcfail[i]);
						}
						//Se cierra el archivo.
						if(ipg!=NULL) fclose(ipg);
						//Se le hace unlock al mutex de los errores.
						pthread_mutex_unlock(&emutex);
					
					}
					//dump de transacciones generadas.
					else if (!strncmp("dump", readbuffer, strlen("dump"))){
						//Se le hace lock al mutex de las transacciones, para que no se puedan agregar transacciones mientras se hace el dump.
						pthread_mutex_lock(&tmutex);
						//Se le pone el nombre al archivo con el id de la sucursal.
						sprintf(name,"dump_%s.csv",branches[forks]);
						//Se crea el archivo.
						ipg=fopen(name, "w");
					        if (ipg == NULL) {
						    fprintf(stderr, "No se puede abrir archivo de entrada\n");
						    exit(1);
					        }
						fprintf(ipg,"tipo de transacción, medio de origen, cuenta de origen, cuenta de destino\n");
						//Se escriben las transacciones en el archivo, una por una.
						for (int i=0; i<tr; i++){
							fprintf(ipg,"%s \n", transc[i]);
						}
						//Se cierra el archivo.
						if(ipg!=NULL) fclose(ipg);
						//Se le hace lock al mutex de las transacciones.
						pthread_mutex_unlock(&tmutex);
					
					}
					else{
					}
					//Se reinicia el buffer devolviendo sus valores a 0.
					memset(&readbuffer[0], 0, sizeof(readbuffer));
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
			//Se le envia el mensaje a todas las sucursales.
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
			//Se le pide el dump a la sucursal determinada.
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
		}else if (!strncmp("dump", commandBuf, strlen("dump"))) {
			char *id;
			if (strlen(commandBuf)<5){
				printf("ERROR Se debe ingresar un id de sucursal\n");
				continue;
			}
			id=strtok(commandBuf, " ");
			id=strtok(NULL," ");
			//Se le pide el dump a la sucursal determinada.
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
	
	//señal para terminar thread del banco matriz concurrente.
	terminated = 1;
	//Se libera el malloc de commandBuf.
	free(commandBuf);
  	printf("Terminando ejecucion limpiamente...\n");
	//Se espera que terminen todas las sucursales.
	wait(NULL);
	//Se hace join del thread del banco matriz. Espera que este termine antes de salir del programa.
	pthread_join(th,NULL);
  	return(EXIT_SUCCESS);
}
