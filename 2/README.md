#Logger

- В качестве процесса можно использовать программу test_app
- Если будет 50 неудачных попыток запуска программы, то в течении часа она запускаться не будет.
- Все сообщения выводятся через syslog
- Умеет перечитывать конфиг, получив сигнал HUP