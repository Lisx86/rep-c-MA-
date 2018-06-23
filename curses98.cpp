#include <curses.h>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <time.h>
#include <vector>


namespace {
    enum Action { Wait, Refresh };
    typedef std::pair<int, int> P;
    std::pair<std::pair<int, int>, bool> operator+(std::pair<int, int> p) {
        return {p, true};
    }
    class Fld {
    public:
        Fld():mainwin(0), row(0), col(0) {
            if ( (mainwin = initscr()) == NULL ) {
                fprintf(stderr, "Error initialising ncurses.\n");
                ::raise(9);
            }
            curs_set( 0); // mainwin,
            ::wmove(mainwin, row, col);
        }
        ~Fld() {
            wgetch(mainwin);
            delwin(mainwin);
            endwin(); /// mainwin??
        }
        friend Fld& operator<<(Fld& lhs, Action action) {
            if(action == Refresh)
                lhs.update();
            else if(action == Wait)
                ::usleep(250000);

            return lhs;
        }
        Fld& operator()(int row, int col) {
            mov(std::make_pair(row, col));
            return *this;
        }
        Fld& operator[](std::pair<int, int> p) {
            shift(p);
            return *this;
        }
        friend Fld& operator<<(Fld& lhs, const std::string& str) {
            lhs.draws(str);
            return lhs;
        }
        int rows() {return getmaxy(mainwin);}
        int cols() {return getmaxx(mainwin);}
        friend std::pair<std::pair<int, int>, bool> operator+(std::pair<int, int> p);
    private:
        void update() {
            ::wrefresh(mainwin);
        }
        void mov(std::pair<int, int> cord) {
            row = cord.first;
            col = cord.second;
            ::wmove(mainwin, row, col);
        }
        void shift(std::pair<int, int> cord) {
            col += cord.second;
            row += cord.first;
            ::wmove(mainwin, row, col);
        }
        void drawc(char ch) {
            ::waddch(mainwin, ch);
        }
        void draws(const std::string& str) {
            ::waddstr(mainwin, str.c_str());
        }

        WINDOW* mainwin;
        int row, col; // getcuryx  doesn't work (55 ---01--> 56      66 ---01--> 67      77 ---01--> 78 )!  adding current coords to state
    };
}
    class Digit {
    public:
        struct Element{
            Element(P p, bool vert, bool enab):p(p),vert(vert),enab(enab) {
            }
            P p;
            bool vert;
            bool enab;
        };
        Digit(int row0, int col0, unsigned char* data) {
            base.push_back(Element(std::make_pair(row0+0,  col0 + 2), false, *(data+0) ));
            base.push_back(Element(std::make_pair(row0+2,  col0 + 0), true,  *(data+1) ));
            base.push_back(Element(std::make_pair(row0+2,  col0 +12), true,  *(data+2) ));
            base.push_back(Element(std::make_pair(row0+6,  col0 + 2), false, *(data+3) ));
            base.push_back(Element(std::make_pair(row0+8,  col0 + 0), true,  *(data+4) ));
            base.push_back(Element(std::make_pair(row0+8,  col0 +12), true,  *(data+5) ));
            base.push_back(Element(std::make_pair(row0+12, col0 + 2), false, *(data+6) ));
        }
        void sync(Fld& f) {
            for(int cn = 0; cn < base.size(); cn++) {
                Element& e = base[cn];
                if(e.vert) {
                   int row = e.p.first-1;
                   int col = e.p.second;
                   for(int sc = 0; sc < 5; sc ++ )
                      f(row + sc, col) << (e.enab ? "o" : " ");
                }else if(!e.vert) {
                    int row = e.p.first;
                    int col = e.p.second;
                    for(int sc = 0; sc < 9; sc ++ )
                        f(row, col+sc) << (e.enab ? "o" : " ");
                }//ei
            }//for
         }
    private:
        std::vector<Element> base;
    };

void sync(unsigned char* data, Fld& f) {
    int row0=8, col0=24;
    Digit d0(row0+0, col0+0,  data)
         ,d1(row0+0, col0+15, data+7)
         ,d2(row0+0, col0+35, data+7+7)
         ,d3(row0+0, col0+50, data+7+7+7);
    d0.sync(f);
    d1.sync(f);
    d2.sync(f);
    d3.sync(f);
}
bool setone(unsigned char* data, int which, int value) {
    bool bad = !data || which<0 || which > 3 || value < 0 || value > 9;
    int shift = 7*which;
    for(int cn = 0; cn < 7; cn++)
        data[shift+cn] = 0;

    data[shift+0]= !(bad || value == 4 || value == 1 );
    data[shift+1]= !(bad || value == 1 || value == 2 || value == 3 || value == 7);
    data[shift+2]= !(bad || value == 5 || value == 6);
    data[shift+3]= !(bad || value == 0 || value == 1|| value == 7);
    data[shift+4]= !(bad || value == 1 || value == 3|| value == 4|| value == 5|| value == 7 || value == 9);
    data[shift+5]= !(bad || value == 2);
    data[shift+6]= !(bad || value == 1|| value == 4|| value == 7);
}
void split(unsigned char* digits, int h , int m) {
    digits[0] = digits[1] = 0;
    if(h>9 && h < 24) {
        digits[0] = h / 10;
        digits[1] = h % 10;
    }else if(h >= 0 && h < 10) {
        digits[1] = h;
    }

    digits[2] = digits[3] = 0;
    if(m>9 && m < 59) {
        digits[2] = m / 10;
        digits[3] = m % 10;
    }else if(m >= 0 && m < 10) {
        digits[3] = m;
    }
}

void showtime(int h, int m, Fld& f) {
    unsigned char dig[4] = {0};
    split(dig, h, m);

    unsigned char data[28] = {0};
    setone(data, 0, dig[0]);
    setone(data, 1, dig[1]);
    setone(data, 2, dig[2]);
    setone(data, 3, dig[3]);
    sync(data, f);
    f << Refresh;
}

int main(int argc, char* argv[]) {
    Fld f;
    for(int m = 0, h= 0; true; ) {
        showtime(h, m, f);
        ::usleep(100000);
	if(++m == 60) {
		m=0;
	       	if(++h == 24)
 	  	   h = 0;
	}
	
    }

    return 0;
}

