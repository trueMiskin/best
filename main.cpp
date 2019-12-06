#include <cstdio>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
using namespace std;

#include "common.h"
#include "marshal.h"
#include "update.h"
#include <queue>

int pocet_hracov, moje_id;
Mapa mapa;
Stav stav;
Hrac ja; // da sa ziskat aj ako stav.hraci[moje_id]
vector<Tah> historia; // da sa ziskat aj ako stav.historia[moje_id];

// pomocne funkcia na vytvorenie prikazov
Prikaz prikazChod(int nove_x, int nove_y) {
  Prikaz prikaz;
  prikaz.typ = CHOD;
  prikaz.parametre[0] = nove_x;
  prikaz.parametre[1] = nove_y;
  return prikaz;
}

Prikaz prikazChod(Pozicia pozice) {
  return prikazChod(pozice.x, pozice.y);
}

Prikaz prikazLiecSa() {
  Prikaz prikaz;
  prikaz.typ = LIEC_SA;
  return prikaz;
}

Prikaz prikazOtocSa(int novy_smer) {
  Prikaz prikaz;
  prikaz.typ = OTOC_SA;
  prikaz.parametre[0] = novy_smer;
  return prikaz;
}

Prikaz prikazVystrel(int ciel_x, int ciel_y) {
  Prikaz prikaz;
  prikaz.typ = VYSTREL;
  prikaz.parametre[0] = ciel_x;
  prikaz.parametre[1] = ciel_y;
  return prikaz;
}

Prikaz prikazZdvihni(int index_itemu) {
  Prikaz prikaz;
  prikaz.typ = ZDVIHNI;
  prikaz.parametre[0] = index_itemu;
  return prikaz;
}

Prikaz prikazZmenZbran() {
  Prikaz prikaz;
  prikaz.typ = ZMEN_ZBRAN;
  return prikaz;
}

//Pomocna funkcia, ktora vam povie kadial by letel naboj, ak vystrelite na (ciel_x, ciel_y)
vector<Pozicia> drahaNaboja(int ciel_x, int ciel_y, bool kontroluj_steny=true, bool kontroluj_krabice=true){
  if (Pozicia(ciel_x,ciel_y) == ja.pozicia) {
    fprintf(stderr,"drahaNaboja: argument rovnaky ako ja.pozicia\n");
    return vector<Pozicia>();
  }
  if (ciel_x < 0 || ciel_x >= stav.mapa.w || ciel_y < 0 || ciel_y >= stav.mapa.h) {
    fprintf(stderr,"drahaNaboja: argument mimo mapy %d %d\n", ciel_x, ciel_y);
    return vector<Pozicia>();
  }
  
  int hlavny_smer;
  int dx = ciel_x - ja.pozicia.x, dy = ciel_y - ja.pozicia.y;
  int max_skalarny_sucin = 0;
  for (int smer = 0; smer < 4; smer++) {
    int skalakalarny_sucin = kDx[smer] * dx + kDy[smer] * dy;
    if(max_skalarny_sucin < skalakalarny_sucin){
      max_skalarny_sucin = skalakalarny_sucin;
      hlavny_smer = smer;
    }
  }
    
  int vedlajsi_smer = -1;
  int vedlajsi_skalarny_sucin = 0;
  vector<int> kandidati = {(hlavny_smer + 3) % 4, (hlavny_smer + 1) % 4};
  for (int smer : kandidati) {
    int skalakalarny_sucin = kDx[smer] * dx + kDy[smer] * dy;
    if (skalakalarny_sucin >= 0) {
      vedlajsi_smer = smer;
      vedlajsi_skalarny_sucin = skalakalarny_sucin;
    }
  }
  
  Naboj naboj;
  naboj.pozicia = ja.pozicia;
  naboj.sila = 1;
  naboj.pokles_sily = 0;
  naboj.hlavny_smer = hlavny_smer;
  naboj.vedlajsi_smer = vedlajsi_smer;
  naboj.frekvencia_citatel = vedlajsi_skalarny_sucin;
  naboj.frekvencia_menovatel = max_skalarny_sucin;
  naboj.faza = naboj.frekvencia_menovatel / 2;

  vector<Pozicia> draha;
  bool naboj_zmizne=false;
  while(not naboj_zmizne){
    draha.push_back(naboj.pozicia);

    naboj.pozicia.x += kDx[naboj.hlavny_smer];
    naboj.pozicia.y += kDy[naboj.hlavny_smer];
    naboj.faza += naboj.frekvencia_citatel;
    if (naboj.faza >= naboj.frekvencia_menovatel) {
      naboj.faza -= naboj.frekvencia_menovatel;
      naboj.pozicia.x += kDx[naboj.vedlajsi_smer];
      naboj.pozicia.y += kDy[naboj.vedlajsi_smer];
    }

    if (naboj.pozicia.x < 0 || naboj.pozicia.x >= stav.mapa.w || naboj.pozicia.y < 0 || naboj.pozicia.y >= stav.mapa.h) {
      naboj_zmizne = true;
    }
    else if (kontroluj_krabice && stav.krabice[naboj.pozicia.y][naboj.pozicia.x]) {
      naboj_zmizne = true;
    } 
    else if (kontroluj_steny && mapa.teren[naboj.pozicia.y][naboj.pozicia.x] == STENA) {
      naboj_zmizne = true;
    }
  }
  return draha;
}

//Pomocna funkcia, ktora vam povie, ci medzi vami a miestom na ktore chcete strielat je stena
bool zavadziaStena(int ciel_x, int ciel_y){
  vector<Pozicia> draha=drahaNaboja(ciel_x, ciel_y, true, false);
  for(int i=0; i<draha.size(); i++){
    if(Pozicia(ciel_x,ciel_y)==draha[i]) return false;
  }
  return true;
}

//Pomocna funkcia, ktora vam povie, ci medzi vami a miestom na ktore chcete strielat je krabica
bool zavadziaKrabica(int ciel_x, int ciel_y){
  vector<Pozicia> draha=drahaNaboja(ciel_x, ciel_y, false, true);
  for(int i=0; i<draha.size(); i++){
    if(Pozicia(ciel_x,ciel_y)==draha[i]) return false;
  }
  return true;
}

//Pomocna funkcia, ktora vam vrati vzdialenost bodu od vas, podla ktorej naboje stracaju silu
int vzdialenostPreNaboj(int ciel_x, int ciel_y){
  return max(abs(ja.pozicia.x-ciel_x),abs(ja.pozicia.y-ciel_y));
}

// main() zavola tuto funkciu, ked nacita mapu (v tomto momente este neviete, kde ste)
void inicializuj() {
  // (sem patri vas kod)
}

bool vMape(int x, int y){
  return 0 <= x && x < stav.mapa.w
    && 0 <= y && y < stav.mapa.h;
}

// jestli jsme v zone ci ne, border je zmenseny o 2 v obou smerech
bool vZone(int x, int y){
  return stav.zona_x1 < x && x < stav.zona_x2 - 1
    && stav.zona_y1 < y && y < stav.zona_y2 - 1;
}

bool vidimPolicko(int x, int y){
  return abs(x - ja.pozicia.x) <= kDohladScope[ja.scope]
    && abs(y - ja.pozicia.y) <= kDohladScope[ja.scope];
}

double kPriorita[4][5] = {
    {0, 1, 0.1, 0.2, 0.99}, //zbrane
    {0, 0.2, 0.3, 0.4}, //vesta
    {0, 0.05, 0.1}, //scope
    {0.15} // lekarnicka
};

vector<vector<int>> spocitatVzdalenost(){
  const int MX = 1e9;
  queue<pair<Pozicia, int>> fronta;
  vector<vector<bool>> videno(mapa.h, vector<bool>(mapa.w, false));
  vector<vector<int>> vzdalenost(mapa.h, vector<int>(mapa.w, MX));
  fronta.push({ja.pozicia, 0});

  while(!fronta.empty()){
    auto el = fronta.front(); fronta.pop();
    Pozicia po = el.first;
    int delka = el.second;
    if(mapa.teren[po.y][po.x] != Policko::VOLNE){
      continue;
    }
    if(vzdalenost[po.y][po.x] != MX)
      continue;

    vzdalenost[po.y][po.x] = delka;

    for(int i = 0; i < 4; i++){
      if(vMape(po.x + kDx[i], po.y + kDy[i])){
        fronta.push({Pozicia(po.x + kDx[i], po.y + kDy[i]), delka + 1});
      }
    }
  }

  return vzdalenost;
}

bool mamZbran(int typ){
  return ja.zbrane[0] == typ || ja.zbrane[1] == typ;
}

bool horsiVesta(int typ){
  return ja.vesta < typ;
}

bool horsiScope(int typ){
  return ja.scope < typ;
}

double prioritaProItem(TriedaItemu trieda, int typ){
  if(trieda == TriedaItemu::ZBRAN){
    return kPriorita[0][typ] * mamZbran(typ);
  }else if(trieda == TriedaItemu::VESTA){
    return kPriorita[1][typ] * horsiVesta(typ);
  }else if(trieda == TriedaItemu::SCOPE){
    return kPriorita[2][typ] * horsiScope(typ);
  }else{
    return kPriorita[3][typ];
  }
}

inline double prioritaProItem(Item item){
  return prioritaProItem(item.trieda, item.typ);
}

Pozicia add(Pozicia pozice, int smer){
  return Pozicia(pozice.x + kDx[smer], pozice.y + kDy[smer]);
}

Prikaz prikazVystrel(Pozicia poz){
  return prikazVystrel(poz.x, poz.y);
}

Prikaz otocSeNeboPohni(Pozicia poz, int smer){
      if(ja.smer == smer)
        return prikazChod(add(ja.pozicia, smer));
      else{
        return prikazOtocSa(smer);
      }
}

// Najde posledni pohyb, jak se dostat nejrychleji k mistu
// POZOR: na pohyb hrace se tato hodnota musi invertovat
// - pokud jsem se dostal na posledni policko, ze jsem sel dolu, tak spravna cesta vede nahoru
int najdiPosledniPohyb(vector<vector<int>>& vzdalenosti, Pozicia pozice){
  int lastPohyb = -1;
  int vzdalenost = vzdalenosti[pozice.y][pozice.x];
  cerr << "Moje pozice " << ja.pozicia.x << " " << ja.pozicia.y << endl;
  while(!(pozice == ja.pozicia)){
    cerr << "Pozice pohled " << pozice.x << " " << pozice.y << endl;
    cerr << (pozice == ja.pozicia) << endl;
    for(int i = 0; i < 4; i++){ 
      Pozicia novaPozicia = add(pozice, i);
      if(!vMape(novaPozicia.x, novaPozicia.y))
        continue;
      int novaVzd = vzdalenosti[novaPozicia.y][novaPozicia.x];
      cerr << "vzd " << vzdalenost << " " << novaVzd << endl;
      if(vzdalenost > novaVzd){
        vzdalenost = novaVzd;
        lastPohyb = i;
        pozice = novaPozicia;
        break;
      }
    }
  }
  return lastPohyb;
}

inline int invertovatSmer(int smer){
  return (4 + smer - 2)%4;
}

// main() zavola tuto funkciu, ked chce vediet, ake prikazy chceme vykonat
Prikaz zistiTah() {
  cerr << vZone(ja.pozicia.x, ja.pozicia.y) << endl;
  if(!vZone(ja.pozicia.x, ja.pozicia.y)){
    for(int i = 0; i < 4; i++){
      if(vMape(kDx[i] + ja.pozicia.x, kDy[i] + ja.pozicia.y)
          && vZone(kDx[i] + ja.pozicia.x, kDy[i] + ja.pozicia.y)){
        return prikazChod(ja.pozicia.x + kDx[i], ja.pozicia.y + kDy[i]);
      }
    }
  }else{
    auto vzdalenosti = spocitatVzdalenost();

    for(int y = 0; y < mapa.h; y++){
      for(int x = 0; x < mapa.w; x++){
        cerr << vzdalenosti[y][x] <<" ";
      }
      cerr << endl;
    }

    // TODO: Pokud stoji blizko nepritel a ja mam zbran na dalku - strilet

    
    //
    // Sebrani predmetu
    //
    int index = -1;
    for(int i = 0; i < stav.itemy[ja.pozicia.y][ja.pozicia.x].size(); i++){
      if(i == 0)
        index = 0;
      else{
        Item bestItemNow = stav.itemy[ja.pozicia.y][ja.pozicia.x][index];
        Item item = stav.itemy[ja.pozicia.y][ja.pozicia.x][i];
        if(prioritaProItem(item) > prioritaProItem(bestItemNow)){
          index = i;
        }
      }  
    }
    if(index != -1){
      return prikazZdvihni(index);
    }
    

    //
    // Najiti predmetu
    //
    cerr << "Dobre" << endl;
    Pozicia pozice = Pozicia(-1, -1);
    double bestP = -1;
    int dohled = kDohladScope[ja.scope];
    for(int w = max(0, ja.pozicia.x - dohled); w < min(mapa.w, ja.pozicia.x + dohled); w++){
      for(int h = max(0, ja.pozicia.y - dohled); h < min(mapa.w, ja.pozicia.y + dohled); h++){
        vector<Item>& itemy = stav.itemy[h][w];

        if(!vZone(w, h))
          continue;

        for(Item item : itemy){
          double priorita = prioritaProItem(item.trieda, item.typ);

          int vzd = vzdalenosti[h][w];
          priorita = priorita - (vzd * 0.02);
          

          if(bestP < priorita){
            pozice = Pozicia(w, h);
            bestP = priorita;
          }
        }
      }
    }

    //
    // Najiti cesty k nejlepsimu predmetu
    //
    cerr << "Nalezen predmet sour " << pozice.x << " " << pozice.y << endl;
    int lastPohyb = -1;
    if(bestP != -1){
      lastPohyb = najdiPosledniPohyb(vzdalenosti, pozice);
    }

    cerr << "Last pohyb " << lastPohyb << endl;
    if(lastPohyb != -1){
      int reverzniPohyb = invertovatSmer(lastPohyb);
      if(ja.smer == reverzniPohyb)
        return prikazChod(add(ja.pozicia, reverzniPohyb));
      else{
        return prikazOtocSa(reverzniPohyb);
      }
    }
    cerr << "Zkousim najit krabici" << endl;
    //
    // Najiti krabice
    //
    pozice = Pozicia(-1, -1);
    for(int w = max(0, ja.pozicia.x - dohled); w < min(mapa.w, ja.pozicia.x + dohled); w++){
      for(int h = max(0, ja.pozicia.y - dohled); h < min(mapa.w, ja.pozicia.y + dohled); h++){
        if(vZone(w, h) && stav.krabice[h][w] && (pozice.x == -1 || vzdalenosti[h][w] < vzdalenosti[pozice.y][pozice.x])){
          pozice = Pozicia(w, h);
        }
      }
    }

    cerr << "Nalezena krabice sour " << pozice.x << " " << pozice.y << endl;
    cerr << "Vzd " << vzdalenosti[pozice.y][pozice.x] << endl;
    if(pozice.x != -1){
      if(vzdalenosti[pozice.y][pozice.x] == 1){
        int smer = -1;
        for(int i = 0; i < 4; i++){
          if(add(ja.pozicia, i) == pozice){
            smer = i;
            break;
          }
        }
        cerr << "Smer ke krabici" << ja.smer << " " << smer << endl;
        if(ja.smer != smer){
          return prikazOtocSa(smer);
        }else{
          return prikazVystrel(add(ja.pozicia, smer));
        }
      }else{
          int smer = najdiPosledniPohyb(vzdalenosti, pozice);
          return otocSeNeboPohni(ja.pozicia, invertovatSmer(smer));
      }
    }


    // TODO: Pokud nevim, kam jit, jit do stedu

    // TODO: Jit nejrychlejsim zpusobem

    //
    // Pokud se dostanu sem - strilim by default
    //
    return prikazVystrel(ja.pozicia.x + kDx[ja.smer], ja.pozicia.y + kDy[ja.smer]);
  }
  //return prikazChod(ja.pozicia.x, ja.pozicia.y);
}


int main() {
  // v tejto funkcii su vseobecne veci, nemusite ju menit (ale mozte).
  srand(time(0) ^ (getpid() << 6));
  
  nacitaj(cin, mapa);
  nacitaj(cin, pocet_hracov);
  nacitaj(cin, moje_id);
  inicializuj();
  stav = Stav(mapa, pocet_hracov);
  
  while (cin.good()) {
    nacitaj(cin, stav);
    ja = stav.hraci[moje_id];
    historia = stav.historia[moje_id];
    Prikaz prikaz = zistiTah();
    uloz(cout, prikaz);
    cout << ".\n" << flush;   // bodka a flush = koniec odpovede
  }
  return 0;
}

