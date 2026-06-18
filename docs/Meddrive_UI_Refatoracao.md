# Refatoração Visual – Meddrive Printer Manager

## Objetivo

Transformar a aplicação atual em uma interface profissional, moderna e corporativa semelhante a softwares SaaS premium e ferramentas administrativas modernas.

A aplicação deve transmitir:

- Profissionalismo
- Tecnologia
- Confiabilidade
- Simplicidade
- Produto corporativo premium

**Não alterar nenhuma regra de negócio existente.**

Modificar apenas:

- Layout
- Componentes visuais
- Espaçamentos
- Tipografia
- Ícones
- Paleta de cores
- Experiência do usuário

---

# Estilo Visual

## Tema

Dark Theme moderno.

Inspirado em:

- Azure Portal
- Visual Studio 2022 Dark
- GitHub Desktop
- Microsoft Dev Home
- JetBrains Rider

---

## Paleta de Cores

### Fundo Principal
`#0F172A`

### Fundo Secundário
`#111827`

### Cards
`#1E293B`

### Bordas
`#334155`

### Azul Principal
`#3B82F6`

### Azul Hover
`#60A5FA`

### Texto Principal
`#F8FAFC`

### Texto Secundário
`#94A3B8`

### Texto Desabilitado
`#64748B`

---

# Janela Principal

## Remover

A aparência padrão do Windows antigo.

Substituir por:

- Barra superior customizada
- Cantos arredondados
- Sombras suaves
- Layout moderno

## Cabeçalho

Altura: `72px`

### Esquerda

- Ícone da aplicação
- Meddrive Printer Manager

Fonte:

- Segoe UI Semibold
- 18px

### Direita

Botões:

- Minimizar
- Maximizar
- Fechar

Com hover moderno.

---

# Navegação

Substituir abas antigas.

Criar menu horizontal.

## Impressoras

- Ícone: 🖨️
- Texto: IMPRESSORAS

## Configurações

- Ícone: ⚙️
- Texto: CONFIGURAÇÕES

### Aba ativa

Exibir:

- Fundo destacado
- Linha azul inferior
- Texto branco

---

# Área Principal

Adicionar espaçamento interno de `24px`.

---

# Tabela

Modernizar completamente o DataGridView/ListView existente.

## Cabeçalho

Altura: `56px`

Fundo: `#1E293B`

Colunas:

- Porta (ícone de conexão)
- Impressora (ícone de impressora)
- Nome do Arquivo (ícone de documento)
- Pasta de Destino (ícone de pasta)

## Linhas

Altura: `44px`

Hover: `#243244`

Selecionada: `#1D4ED8`

Texto: `#E2E8F0`

Sem grades visuais agressivas.

---

# Estado Vazio

Quando não houver impressoras cadastradas.

Exibir conteúdo centralizado.

## Ícone

- Impressora outline grande
- Tamanho: `96px`
- Cor: `#3B82F6`

## Texto Principal

"Nenhuma impressora cadastrada"

Fonte:

- 20px
- SemiBold

## Texto Secundário

"Clique em 'Adicionar' para cadastrar uma nova impressora."

Fonte:

- 14px

---

# Botões de Ação

Reposicionar abaixo da tabela.

## Adicionar

Botão primário.

Cor: `#2563EB`

Ícone: +

Texto: Adicionar

## Remover

Botão secundário.

Cor: `#374151`

Ícone: -

Texto: Remover

## Atualizar

Botão secundário.

Ícone: ↻

Texto: Atualizar

### Características

Todos os botões devem possuir:

- Cantos arredondados (8px)
- Hover animado
- Transição suave
- Cursor pointer

---

# Barra de Status

Altura: `36px`

Fundo: `#0B1220`

Borda superior azul.

Exibir:

- Ícone de informação
- Quantidade de impressoras cadastradas

Exemplo:

"0 impressoras cadastradas"

Atualizado automaticamente.

---

# Configurações

Criar visual baseado em cards.

Cada configuração dentro de um card moderno.

Exemplo:

- Caminho padrão de saída
- C:\PDF

---

# Tipografia

Utilizar:

- Segoe UI
ou
- Inter

## Hierarquia

### Título

- 18px
- SemiBold

### Subtítulo

- 14px

### Conteúdo

- 13px

---

# Ícones

Substituir todos os ícones antigos.

Utilizar:

- Fluent UI Icons
ou
- Font Awesome

Preferencialmente Fluent UI.

---

# Animações

Adicionar:

### Hover em botões

150ms

### Hover em linhas

150ms

### Troca de abas

200ms fade

### Entrada da tela

Fade-in suave

---

# Responsividade

A interface deve funcionar corretamente em:

- 1280x720
- 1366x768
- 1920x1080
- 4K

Sem componentes sobrepostos.

---

# Requisitos Técnicos

IMPORTANTE:

- NÃO alterar regras de negócio.
- NÃO alterar fluxo de impressão.
- NÃO alterar integração com Ghostscript.
- NÃO alterar monitor de porta.
- NÃO alterar DLLs existentes.
- NÃO alterar persistência de dados.
- NÃO alterar banco de dados.
- NÃO alterar registro do Windows.

Somente refatorar a camada visual.

---

# Entregáveis

1. Refatorar todos os Forms/Telas.
2. Criar tema centralizado reutilizável.
3. Criar classe de cores e estilos.
4. Padronizar todos os controles.
5. Implementar modo Dark corporativo.
6. Melhorar UX geral.
7. Garantir compatibilidade com Windows 10 e Windows 11.
8. Gerar código limpo e organizado.
9. Documentar cada alteração visual realizada.
10. Fornecer antes/depois de cada tela modificada.

## Resultado Esperado

O resultado final deve parecer um software corporativo moderno lançado em 2025, mantendo integralmente a funcionalidade atual do Meddrive Printer Manager.
