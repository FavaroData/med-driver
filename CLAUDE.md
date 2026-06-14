# med-driver — instruções para o Claude

## Regras de trabalho

**Nunca modifique código sem pedir permissão ao usuário antes.**
Apresente a proposta, aguarde aprovação explícita, só então edite os arquivos.

## Formato de resposta ao investigar problemas

Ao analisar logs ou depurar um problema, estruture a resposta assim:

**O que os logs revelaram:**
- liste o que foi observado nos dados/logs
- destaque anomalias (ex: função chamada duas vezes, timeout, ausência de chamada esperada)

**O que foi corrigido:**
- liste cada mudança feita (arquivo + motivo)
- explique a causa raiz em uma linha
