# Простая чат-комната (вариант 4)

В чат-комнате есть 2 типа пользователей – обычные пользователи и администраторы. 
В пределах чат-комнаты пользователи могут обмениваться между собой сообщениями, любое сообщение, не начинающееся  
с "\", посылается всем подключенным пользователям, включая отправителя, в формате: From <nickname>: <message>

Помимо этого, пользователи могут серверу различные команды (список команд представлен ниже). Команда обязательно  
должна начинаться с "\". Если команда, введёная пользователем отсутсвует в списке, она обрабатывается как обычное  
сообщение.

## Логика работы процесса-сервера:

1. Создает "слушающий" сокет и ждет запросов на соединение от клиентов;

2. При поступлении запроса, устанавливается соединение с очередным клиентом, после этого 
сервер ждёт от клиента ввод никнейма и клиент заносится в список присутствующих под введеным им никнеймом.
Сервер также объявляет в общем чате о подключении нового пользователя в формате: *** <nickname> has entered that chatroom

3. От разных клиентов могут поступать реплики – получив реплику от клиента, сервер рассылает
ее всем "присутствующим" (включая самого автора) с указанием автора реплики;

4. При разрыве связи (команда \quit) с клиентом сервер сообщает всем, что-такой-то (имя) нас
покинул (ушел) и выводит его прощальное сообщение в формате: 

## Процесс-сервер умеет:

1. Выполнять следующие команды обычных пользователей:
	- \users – получить от сервера список всех пользователей (имена), которые сейчас онлайн;
	- \nick <newnick> – меняет имя пользователя на <newnick>
	- \admin – после ввода этой команды, сервер запрашивает у пользователя пароль, и если введённый пароль  
	совпадает с паролем администратора, то пользователь наделяется правами администратора
	- \quit [<message>] – выход из чата с прощальным сообщением (поле message необязательно).

2. Выполнять следующие команды пользователей-администраторов:
	- \ban <nickname> <message> – отключает текущего пользователя с заданным именем и запрещает
	новые подключения к серверу с таким именем, при этом пользователю выводится причина его
	блокировки, указанная в <message>;
	- \kick <nickname> <message> – отключает пользователя с заданным именем, при этом
	пользователю выводится причина его блокировки, указанная в <message> ;
	- \nick <oldnickname> <newnickname> – принудительно меняет имя пользователю;
	- \shutdown <message> – завершает работу сервера, а всем пользователям отправляется заданное
	сообщение.

### Замечания:
1. Администраторы не могут влиять на других администраторов.
2. При попытке входа под  зарегистрированным именем, сервером выдается сообщение об ошибке.  
Также сообщение об ошибке выдается при попытке смены старого имени на уже зарегистрированное.
3. Команды, в отличие от сообщений, не рассылаются другим пользователям (т.е остаются известными только серверу).
4. При попытке вызова обычным пользователем команды, доступной только администраторам, сервер ответит, что команда не найдена.
5. Если количество аргументов переданных вместе с командой больше необходимого, лишние аргументы игнорируются, если меньше, то выдается сообщение об ошибке.


## Структуры данных

На сервере информация о каждом пользователе будет храниться в типе struct. 

Поля структуры будут хранить:
1. Имя пользователя  - строка не более 100 символов длиной (с учетом \0)
2. Его статус  ('a' - admin, 'u' - normal user)
3. Фаиловый дескриптор сокета, который используется для взаимодействия с этим клиентом.
4. Статус подключения (1 - подключен, 0-отключен)
5. Статус никнейма (1- никнейм введен, 0 -ожидается ввод никнейма)
6. Статус пароля (1-ожидается ввод пароля администратора, 0 - ввод пароля администратора не ожидается)

Сервер хранит список забаненных пользователей в виде массива строк
Сервер хранит массив структур, в каждой их которых хранится название команды и ссылка на функцию, которая отвечает за ее обработку.


## Интерфейс взаимодействия клиента с сервером

Взаимодействие между сервером и клиентом осуществляется за счёт передачи сообщений длиной не более 1024 символов (длина подсчитывается с учетом \0, доп. звездочек в начале сообщения и без учета \n,т.к \n удаляется из сообщения автоматически). Помимо \n из сообщения автоматически удаляются начальные пробелы

Все сообщения является однострочными, т.е после ввода enter сообщение считается введенным и передается в фукнцию отправки
Каждое сообщение обязательно заканчивается \0
Пустое сообщение (сообщение в котором нет никаких других символов, кроме \n и/или \0) отправке на сервер не подлежит
При вводе слишком длинного сообщения, программа клиента выдает ошибку и сообщение не отправляется


