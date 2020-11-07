#include "9cc.h"

// 今みているトークン
Token *token;

// 入力プログラム
char *user_input;

// エラー箇所を報告する.
void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号のときには、
// トークンを1つ読み進めて真を返す。
// それ以外の場合には偽を返す。
bool consume(char *op)
{
    if (
        token->kind != TK_RESERVED ||
        strlen(op) != token->len ||
        memcmp(token->str, op, token->len))
    {
        return false;
    }
    token = token->next;
    return true;
}

// 次のトークンが期待している 記号 の場合トークンを1つ読み進め，
// それ以外の場合にはエラーになる
void expect(char *op)
{
    if (
        token->kind != TK_RESERVED ||
        token->len != strlen(op) ||
        memcmp(token->str, op, token->len))
    {
        error_at(token->str, "'%s'ではありません", op);
    }
    token = token->next;
}

// 次のトークンが 数値 の場合トークンを1つ読み進めてその数値を返す．
// それ以外の場合にはエラーになる
int expect_number()
{
    if (token->kind != TK_NUM)
    {
        error_at(token->str, "数ではありません");
    }
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof()
{
    return token->kind == TK_EOF;
}

// 新しいトークンを作成して cur につなげる
Token *new_token(TokenKind kind, Token *cur, char *str, int len)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool startswith(char *p, char *q)
{
    return memcmp(p, q, strlen(q)) == 0;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p)
{
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p)
    {
        if (isspace(*p))
        {
            p++;
            continue;
        }

        if (
            startswith(p, "==") ||
            startswith(p, "!=") ||
            startswith(p, "<=") ||
            startswith(p, ">="))
        {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p+=2;
            continue;
        }

        if (strchr("+-*/()<>", *p))
        {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if (isdigit(*p))
        {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len > p - q;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

// 目標
//
// 扱いたい演算(記号)
// == !=
// < <= > >=
// + -
// * /
// 単項+ 単項-
// ()
//
// 演算の優先順序
// expr       = equality
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add        = mul ("+" mul | "-" mul)*
// mul     = primary ("*" primary | "/" primary)*
// unary   = ("+" | "-")? primary
// primary = num | "(" expr ")"
//

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

// 2項演算子用
Node *new_node_binary(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

// 1項演算子用
Node *new_node_num(int val)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

// expr = equality
Node *expr()
{
    return equality();
}

// equality  = relational ("==" relational | "!=" relational)*
Node *equality()
{
    Node *node = relational();
    for (;;)
    {
        if (consume("=="))
        {
            node = new_node_binary(ND_EQ, node, relational());
        }
        else if (consume("!="))
        {
            node = new_node_binary(ND_NE, node, relational());
        }
        else
        {
            return node;
        }
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational()
{
    Node *node = add();
    for (;;)
    {
        if (consume("<"))
        {
            node = new_node_binary(ND_LT, node, add());
        }
        else if (consume("<="))
        {
            node = new_node_binary(ND_LE, node, add());
        }
        else if (consume(">"))
        {
            node = new_node_binary(ND_LT, add(), node);
        }
        else if (consume(">="))
        {
            node = new_node_binary(ND_LE, add(), node);
        }
        else
        {
            return node;
        }
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add()
{
    Node *node = mul();
    for (;;)
    {
        if (consume("+"))
        {
            node = new_node_binary(ND_ADD, node, mul());
        }
        else if (consume("-"))
        {
            node = new_node_binary(ND_SUB, node, mul());
        }
        else
        {
            return node;
        }
    }
}

// mul = primary ("*" primary | "/" primary)*
Node *mul()
{
    Node *node = unary();
    for (;;)
    {
        if (consume("*"))
        {
            node = new_node_binary(ND_MUL, node, unary());
        }
        else if (consume("/"))
        {
            node = new_node_binary(ND_DIV, node, unary());
        }
        else
            return node;
    }
}

// unary   = ("+" | "-")? primary
Node *unary()
{
    if (consume("+"))
        return primary();
    if (consume("-"))
        return new_node_binary(ND_SUB, new_node_num(0), primary());
    return primary();
}

// primary = num | "(" expr ")"
Node *primary()
{
    // "(" expr ")" or 数値 じゃなきゃダメ！
    if (consume("("))
    {
        Node *node = expr();
        expect(")");
        return node;
    }
    return new_node_num(expect_number());
}
