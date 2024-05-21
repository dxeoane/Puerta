Sketch de Arduino para controlar la apertura de una puerta.

El hardware que se usa es el modulo HW-622 basado en el ESP8266, aunque con pocos cambios debería funcionar con cualquier ESP8266 que tenga un relé conectado.

En la carpeta "App" se incluye una aplicación de escritorio, hecha en Lazarus (Freepascal), que se puede compilar tanto para usar en un PC con Windows, como en un PC con Linux. Esta aplicación muestra un icono en la barra tareas con un menu para abrir la puerta

Tambien se puede enviar directamente la orden de abrir desde la linea de comandos usando netcat: 
```
echo "pulse:key" | netcat -uc 192.168.1.123 61978 
```
Donde "key" es la clave que hemos configurado en el dispositivo, "192.168.1.123" la ip y "61978" el puerto

Para configurar el dispostivo hay que mantener el pin GPIO0 a GND hasta que el led de estado empieze a parpadear

Los comandos de configuración se pueden mandar con netcat:
```
echo "passwd:nuevakey" | netcat -uc 192.168.1.123 61978 
```