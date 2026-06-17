1. Adicionar impressora vai chamar o add-printer.ps1, pois ele está com a lógica de adicionar impressoras isolada do install.ps1 (verifique depois esse arquivo por
  completo para visualizar o que pode faltar, regras de negócio, lógicas para funcionar melhor, e adaptações) 

2. Remover Impressora: remover impressora vai remover 100% a impressora
  cadastrada: tanto do registry, quanto da lista de impressoras. Spooler, etc, tudo referente a aquela impressora que foi adicionada vai ser removido do windows (criar um
  novo arquivo ps1 para isso). 

3. Listagem: a listagem vai continuar sendo um json armazenando as impressoras adicionadas. Mas esse json precisa receber uma lógica de exibição:
  verificar impressoras registradas via enumprinters quando houver alguma alteração (adicionar, remover, editar, refresh), mas se caso não tiver sido chamada nenhuma
  dessas funções, ele vai continuar lendo o json que já estava armazenado antes (ele não deve atualizar seu estado ao abrir ou fechar o aplicativo, mas sim consultar em
  memória como eu falei antes). 

4. Privilégios:  executar uma ação quando precisar de admin
