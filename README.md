# Cliente e Servidor HTTP desenvolvidos em C para a disciplina de Redes de Computadores ministrada pelo prof. Flávio Luiz Schiavoni na Universidade Federal de São João del Rei.

## Cliente HTTP:

O Cliente envia requisições HTTP/1.1 GET para uma URL. Caso a URL especifique diretamente o caminho de um arquivo, o cliente baixa o arquivo na pasta client/. Caso a URL não especifique diretamente o caminho de um arquivo, o cliente busca por index.html e, caso encontre, baixa-o na pasta client/.

### Tutorial de Compilação e Execução:

Para compilar tanto o Servidor quanto o Cliente, basta rodar make na pasta raiz do projeto. Para compilar somente o cliente, basta rodar gcc client.c -o meu_navegador na pasta client/.

Para execução, o cliente recebe apenas a URL como parâmetro. Assim, para executar o cliente, basta rodar ./meu_navegador <URL> na pasta client/.

## Servidor HTTP:

O Servidor recebe requisições GET e oferece arquivos estáticos de qualquer diretório, lidando com requisições concorrentes usando fork(). Se o diretório oferecido possui index.html, o mesmo é servido automaticamente. Se o diretório nao possui index.html, uma listagem HTML dos subdiretórios é retornada. Por padrão, roda na porta 8080, podendo ser alterado.

### Tutorial de Compilação e Execução:

Para compilar tanto o Servidor quanto o Cliente, basta rodar make na pasta raiz do projeto. Para compilar somente o servidor, basta rodar gcc server.c -o meu_servidor na pasta server/.

Para execução, o servidor recebe apenas o caminho do diretório a ser oferecido como parâmetro. Assim, para executar o servidor, basta rodar ./meu_servidor <PATH> na pasta server/.