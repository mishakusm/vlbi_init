Для компиляции прописать 
gcc main.cpp -o init -lstdc++
Компиляция выполняется на ОС FreeBSD 14
Для запуска программы необходимо указать мак адреса источника (-s) и назначения (-d), а также указать сколько секунд генерировать трафик (-t) (необязательный аргумент, программа отсылает пакеты на протяжении 5 секунд по стандарту)


Пример:

./init -d ff:ff:ff:ff:ff:ff -s 08:00:45:27:4b:6e -t 5

Файл Head_example размером 32 байта содержит пример заголовка для vdif файла
