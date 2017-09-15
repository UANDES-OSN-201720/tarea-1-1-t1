# tarea1-1
Tarea 1, parte 1
Grupo Aldunate-Hurtado

1. Funciones principales del programa:

-Matriz Banco:

Main thread: Maneja la interfaz de consola de texto para ingreso de comandos, su manejo, y creación de hijos(sucursales). Puede utilizar los siguientes comandos:

-“list”: entrega una lista con todas las sucursales activas.
-“init”: crea nuevas sucursales. Se le puede entregar un número y así especificar la cantidad de cuentas para este. Si no se entrega un numero o si este excede 10000, por defecto será creada con 1000 cuentas. Adicionalmente se puede entregar un número extra para especificar cuantas terminales (threads) se quiere crear en la sucursal. el maximo de threads es 8. se tiene un default de 1 terminal.(formato: init N° cuentas N° threads)
-“kill”: este comando se le entrega algún numero de una sucursal para poder terminar se procesó. Además, se eliminara de la lista del banco matriz y además de las otras sucursales.
-“dump”: este comando se le entrega algún numero de una sucursal para poder generar un archivo en formato csv. Este contendrá todas las transacciones generadas el thread child de la sucursal especificada.
-“dump_accs”: este comando se le entrega algún numero de una sucursal para poder generar un archivo en formato csv. Este contendrá todas las cuentas y saldos respectivos de estas de la sucursal especificada.
-“dump_accs”: este comando se le entrega algún numero de una sucursal para poder generar un archivo en formato csv. Este contendrá todos los errores que ocurrieron en la sucursal especificada. El primer tipo de error es falta de dinero en cuentas a las cuales se quería hacer un depósito o retiro. El segundo es que la transacción generada fue hecha con una cuenta invalida.
-“quit”: este comando hace terminar el proceso padre. Primero rompe el while de la consola. Luego manda señal “kill” a todas las sucursales que están aún andando. Este utiliza el comando wait(), para esperar que todas las sucursales terminen antes de este terminar finalmente.

Thread parent: Lee todas las transacciones generadas por el thread child de las sucursales emisoras, para verificar si la cuenta existe y si es así redirecciona las operaciones al main thread del proceso de la sucursal receptora, utilizando un pipe especifico del Id de la sucursal a la que es dirigida la operación. También maneja los errores cuando las sucursales cierran, devuelven los depósitos y avisan de errores de cuentas.

-Sucursales:

Main thread: Maneja las instrucciones entregadas por los pipes, ejecutando los comandos correspondientes, tanto los depósitos, retiros, list, dump, etc. Todo lo que le manda el banco matriz es procesado y ejecutado. Creamos unas nuevas funciones para ayudarnos en el manejo de los depósitos, retiros, errores y la creación de nuevas sucursales.

-DP: Utiliza la función addcash para sumar el dinero del depósito a la cuenta.
-RT: Utiliza la función subcash para restar el dinero del retiro a la cuenta, para posteriormente enviar una instrucción de depósito a la cuenta de donde vino la instrucción de retiro. Además, maneja los errores cuando no hay suficiente dinero para efectuar el retiro.
-FAIL: Maneja el error en el momento en que termine una sucursal a la que se le estaba enviando una transacción. Asi se devolverá un error al banco matriz para posteriormente re enviarse a la matriz emisora original.
-NS: Avisa a todas las sucursales cuando se crea una nueva sucursal.

Ahora se crean n thread child, dependiendo si es especificado en el init de la sucursal.
Thread child : Genera las transacciones,  las guarda, y a la vez maneja los errores de montos. También es el encargado de restar el dinero de la cuenta que genera un depósito. 

2. Problemas:
El problema mas grande que nos encontramos fue que al pasar una cierta cantidad de threads, derrepente el banco matriz no leia bien lo enviado por las sucursales. Esto lo arreglamos comenzando a leer lo que se escribia en los pipes caracter por caracter, en vez de linea por linea.

3. Funciones sin implementar:
Implementamos todas las funciones requeridas para la entrega 1 parte 2 de la tarea.
