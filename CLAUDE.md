# CLAUDE.md
# med-driver — instruções para o Claude

## Regras de trabalho

**Nunca modifique código sem pedir permissão ao usuário antes.**
Apresente a proposta, aguarde aprovação explícita, só então edite os arquivos.

**Antes de implementar, mostre o código e as alterações.**
Para cada mudança proposta, escreva o diff exato — arquivo por arquivo, com as linhas removidas e adicionadas — antes de aplicar qualquer edição. O usuário precisa ver e aprovar o código concreto, não apenas a descrição em palavras.

**Sempre explique com muitas palavras.**
Ao responder perguntas, investigar problemas ou apresentar propostas, use linguagem detalhada e completa. Não resuma demais. Prefira clareza e contexto a brevidade.

## Formato de resposta ao investigar problemas

Ao analisar logs ou depurar um problema, estruture a resposta assim:

**O que os logs revelaram:**
- liste o que foi observado nos dados/logs
- destaque anomalias (ex: função chamada duas vezes, timeout, ausência de chamada esperada)

**O que foi corrigido:**
- liste cada mudança feita (arquivo + motivo)
- explique a causa raiz em uma linha

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

1. Foque no "Porquê" (Intenção)O código já diz o que ele faz. O seu comentário deve explicar por que foi feito daquela maneira, especialmente se a solução parecer estranha à primeira vista.Ruim (estilo IA): // Adiciona 1 ao valor de xBom (humano): // Adiciona 1 para compensar o offset do índice da API externa que começa em 1.2. Documente Decisões e "Bugs" (Hackzinhos)Se você teve que fazer uma gambiarra ou contornar um problema do framework, deixe isso claro. Isso evita que outro programador (ou você mesmo no futuro) "otimize" o código e quebre tudo.// Não altere a ordem das linhas abaixo. O navegador X tem um bug de renderização se o elemento Y não for chamado antes.3. Explique Regras de Negócio ComplexasUse os comentários para traduzir a lógica de negócios da vida real para o código.// Aplica a taxa de desconto de 15% apenas se o cliente for VIP e a compra for superior a R$ 500 (Regra definida pelo setor financeiro em 2026).4. Escreva em Português Natural e DiretoEvite formalidades robóticas ou traduções literais. Seja casual, mas profissional.Ruim: // Inicializa a variável the_list para armazenamentoBom: // Guarda os itens temporariamente antes de enviar pro banco5. Use TODOs para Dívidas TécnicasÉ muito humano deixar tarefas para depois. Comentários de TODO mostram que você sabe que algo pode ser melhorado.// TODO: Refatorar essa função para usar async/await quando a biblioteca atualizar.6. Menos é MaisNão comente código autoexplicativo. Se a sua variável se chama tempoExpiracaoDias, você não precisa de um comentário dizendo // Define o tempo de expiração em dias. Poluir o código com comentários óbvios é a maior característica de códigos gerados por IA.A regra de ouro é: escreva o código para ser lido como um livro. Quando o código não for suficiente para explicar a complexidade da situação, aí sim entre com o comentário.