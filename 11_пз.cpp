#include <windows.h>
#include <string>
#include <cctype>
#include <stdexcept>
#include <cstdio>

using namespace std;

// ====================== Класи дерева виразу ======================

class Telement {
protected:
    Telement* left;
    Telement* right;
    Telement(Telement* L, Telement* R) : left(L), right(R) {}
public:
    virtual ~Telement() {
        delete left;
        delete right;
    }
    virtual double result() const = 0;
    virtual Telement* copy() const = 0;
    virtual Telement* differ() const = 0;
    virtual void set_var(double X) {
        if (left)  left->set_var(X);
        if (right) right->set_var(X);
    }
};

class Real : public Telement {
    double f;
public:
    Real(double F) : Telement(nullptr, nullptr), f(F) {}
    double result() const override {
        return f;
    }
    Telement* copy() const override {
        return new Real(f);
    }
    Telement* differ() const override {
        return new Real(0.0);
    }
};

class Var : public Telement {
    double x;
public:
    Var(double X = 0.0) : Telement(nullptr, nullptr), x(X) {}
    double result() const override {
        return x;
    }
    Telement* copy() const override {
        return new Var(x);
    }
    Telement* differ() const override {
        return new Real(1.0);
    }
    void set_var(double X) override {
        x = X;
    }
};

class Plus : public Telement {
public:
    Plus(Telement* L, Telement* R) : Telement(L, R) {}
    double result() const override {
        return left->result() + right->result();
    }
    Telement* copy() const override {
        return new Plus(left->copy(), right->copy());
    }
    Telement* differ() const override {
        return new Plus(left->differ(), right->differ());
    }
};

class Minus : public Telement {
public:
    Minus(Telement* L, Telement* R) : Telement(L, R) {}
    double result() const override {
        return left->result() - right->result();
    }
    Telement* copy() const override {
        return new Minus(left->copy(), right->copy());
    }
    Telement* differ() const override {
        return new Minus(left->differ(), right->differ());
    }
};

class Mult : public Telement {
public:
    Mult(Telement* L, Telement* R) : Telement(L, R) {}
    double result() const override {
        return left->result() * right->result();
    }
    Telement* copy() const override {
        return new Mult(left->copy(), right->copy());
    }
    Telement* differ() const override {
        return new Plus(
            new Mult(left->differ(), right->copy()),
            new Mult(left->copy(), right->differ())
        );
    }
};

class Div : public Telement {
public:
    Div(Telement* L, Telement* R) : Telement(L, R) {}
    double result() const override {
        return left->result() / right->result();
    }
    Telement* copy() const override {
        return new Div(left->copy(), right->copy());
    }
    Telement* differ() const override {
        Telement* num = new Minus(
            new Mult(left->differ(), right->copy()),
            new Mult(left->copy(), right->differ())
        );
        Telement* den = new Mult(right->copy(), right->copy());
        return new Div(num, den);
    }
};

// ====================== Парсер виразів ======================

static string removeSpaces(const string& s) {
    string r;
    for (char c : s) {
        if (!isspace((unsigned char)c)) r.push_back(c);
    }
    return r;
}

static bool canStripOuterParentheses(const string& s) {
    if (s.size() < 2 || s.front() != '(' || s.back() != ')')
        return false;
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        if (depth == 0 && i != s.size() - 1)
            return false;
    }
    return true;
}

static int posFromEndTop(const string& s, char op) {
    int depth = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        char c = s[i];
        if (c == ')') depth++;
        else if (c == '(') depth--;
        if (depth != 0) continue;
        if (c == op) {
            if (op == '-' && i == 0) continue; // унарний мінус ігноруємо
            return i;
        }
    }
    return -1;
}

Telement* form(const string& expr);

Telement* form(const string& expr) {
    string s = removeSpaces(expr);
    if (s.empty()) throw runtime_error("Порожній вираз");

    while (canStripOuterParentheses(s)) {
        s = s.substr(1, s.size() - 2);
    }

    int p = posFromEndTop(s, '+');
    if (p > 0) {
        string s1 = s.substr(0, p);
        string s2 = s.substr(p + 1);
        return new Plus(form(s1), form(s2));
    }

    p = posFromEndTop(s, '-');
    if (p > 0) {
        string s1 = s.substr(0, p);
        string s2 = s.substr(p + 1);
        return new Minus(form(s1), form(s2));
    }

    p = posFromEndTop(s, '*');
    if (p > 0) {
        string s1 = s.substr(0, p);
        string s2 = s.substr(p + 1);
        return new Mult(form(s1), form(s2));
    }

    p = posFromEndTop(s, '/');
    if (p > 0) {
        string s1 = s.substr(0, p);
        string s2 = s.substr(p + 1);
        return new Div(form(s1), form(s2));
    }

    if (s == "x" || s == "X") {
        return new Var();
    }

    double val = 0.0;
    size_t pos = 0;
    try {
        val = stod(s, &pos);
    }
    catch (...) {
        throw runtime_error("Невірний операнд: " + s);
    }
    if (pos != s.size()) {
        throw runtime_error("Невірний операнд: " + s);
    }
    return new Real(val);
}

// ====================== GUI (WinAPI) ======================

HINSTANCE g_hInst;

HWND hEditTimeExpr;    // вираз для часу x
HWND hEditPathExpr;    // вираз для шляху s(x)
HWND hEditResTime;     // результат x
HWND hEditResPath;     // результат s(x)
HWND hEditResVel;      // результат s'(x)
HWND hEditResAcc;      // результат s''(x)
HWND hEditDigits;      // кількість знаків після крапки
HWND hBtnCalc;
HWND hBtnExit;

#define ID_EDIT_TIME_EXPR   201
#define ID_EDIT_PATH_EXPR   202
#define ID_EDIT_RES_TIME    203
#define ID_EDIT_RES_PATH    204
#define ID_EDIT_RES_VEL     205
#define ID_EDIT_RES_ACC     206
#define ID_EDIT_DIGITS      207
#define ID_BTN_CALC         208
#define ID_BTN_EXIT         209

static string getText(HWND h) {
    char buf[512];
    GetWindowTextA(h, buf, sizeof(buf));
    return string(buf);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        CreateWindowExA(0, "BUTTON", "Математична модель",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 10, 460, 90,
            hwnd, NULL, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "час x =",
            WS_CHILD | WS_VISIBLE,
            20, 35, 70, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditTimeExpr = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "1",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            90, 35, 360, 20,
            hwnd, (HMENU)ID_EDIT_TIME_EXPR, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "шлях s(x) =",
            WS_CHILD | WS_VISIBLE,
            20, 65, 70, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditPathExpr = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "x*x/2",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            90, 65, 360, 20,
            hwnd, (HMENU)ID_EDIT_PATH_EXPR, g_hInst, NULL);

        CreateWindowExA(0, "BUTTON", "Результати обчислень",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 110, 460, 140,
            hwnd, NULL, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "час x:",
            WS_CHILD | WS_VISIBLE,
            20, 135, 70, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditResTime = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            90, 135, 360, 20,
            hwnd, (HMENU)ID_EDIT_RES_TIME, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "шлях s(x):",
            WS_CHILD | WS_VISIBLE,
            20, 165, 70, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditResPath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            90, 165, 360, 20,
            hwnd, (HMENU)ID_EDIT_RES_PATH, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "швидкiсть s'(x):",
            WS_CHILD | WS_VISIBLE,
            20, 195, 110, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditResVel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            130, 195, 320, 20,
            hwnd, (HMENU)ID_EDIT_RES_VEL, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "прискорення s''(x):",
            WS_CHILD | WS_VISIBLE,
            20, 225, 120, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditResAcc = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            140, 225, 310, 20,
            hwnd, (HMENU)ID_EDIT_RES_ACC, g_hInst, NULL);

        CreateWindowExA(0, "BUTTON", "Точнiсть",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 260, 300, 70,
            hwnd, NULL, g_hInst, NULL);

        CreateWindowExA(0, "STATIC", "знакiв пiсля крапки:",
            WS_CHILD | WS_VISIBLE,
            20, 285, 130, 20,
            hwnd, NULL, g_hInst, NULL);

        hEditDigits = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "4",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            160, 285, 40, 20,
            hwnd, (HMENU)ID_EDIT_DIGITS, g_hInst, NULL);

        hBtnCalc = CreateWindowExA(0, "BUTTON", "Обчислити",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            330, 270, 130, 25,
            hwnd, (HMENU)ID_BTN_CALC, g_hInst, NULL);

        hBtnExit = CreateWindowExA(0, "BUTTON", "Вихiд",
            WS_CHILD | WS_VISIBLE,
            330, 300, 130, 25,
            hwnd, (HMENU)ID_BTN_EXIT, g_hInst, NULL);
    }
    break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_EXIT && HIWORD(wParam) == BN_CLICKED) {
            PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == ID_BTN_CALC && HIWORD(wParam) == BN_CLICKED) {
            try {
                string timeExpr = getText(hEditTimeExpr);
                string pathExpr = getText(hEditPathExpr);
                string digStr = getText(hEditDigits);

                int digits = 4;
                if (!digStr.empty()) {
                    digits = atoi(digStr.c_str());
                    if (digits < 0) digits = 0;
                    if (digits > 15) digits = 15;
                }

                Telement* xExpr = form(timeExpr);
                Telement* fExpr = form(pathExpr);
                Telement* f1Expr = fExpr->differ();
                Telement* f2Expr = f1Expr->differ();

                double X = xExpr->result();

                fExpr->set_var(X);
                f1Expr->set_var(X);
                f2Expr->set_var(X);

                double xVal = xExpr->result();
                double sVal = fExpr->result();
                double vVal = f1Expr->result();
                double aVal = f2Expr->result();

                char buf[256];
                char format[32];
                sprintf_s(format, "%%.%df", digits);

                sprintf_s(buf, format, xVal);
                SetWindowTextA(hEditResTime, buf);

                sprintf_s(buf, format, sVal);
                SetWindowTextA(hEditResPath, buf);

                sprintf_s(buf, format, vVal);
                SetWindowTextA(hEditResVel, buf);

                sprintf_s(buf, format, aVal);
                SetWindowTextA(hEditResAcc, buf);

                delete xExpr;
                delete fExpr;
                delete f1Expr;
                delete f2Expr;
            }
            catch (const exception& e) {
                MessageBoxA(hwnd, e.what(), "Помилка", MB_OK | MB_ICONERROR);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    FreeConsole();

    g_hInst = GetModuleHandle(NULL);

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "RocketExprWndClass";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Не вдалося зареєструвати клас вiкна", "Помилка", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowExA(
        0,
        "RocketExprWndClass",
        "Полiт ракети: шлях, швидкiсть i прискорення",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 370,
        NULL, NULL, g_hInst, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Не вдалося створити вiкно", "Помилка", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
