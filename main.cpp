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
#include <vector>
#include <algorithm>

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

// jestli jsme v zone ci ne
bool vZone(int x, int y){
  return stav.zona_x1 <= x && x < stav.zona_x2 - 1
    && stav.zona_y1 <= y && y < stav.zona_y2 - 1;
}

bool vidimPolicko(int x, int y){
  return abs(x - ja.pozicia.x) <= kDohladScope[ja.scope]
    && abs(y - ja.pozicia.y) <= kDohladScope[ja.scope];
}

double kPriorita[4][5] = {
    {0, 1, 0.5, 0.3, 0.99}, //zbrane
    {0, 0.2, 0.4, 0.45}, //vesta
    {0, 0.33, 0.38}, //scope
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
    if(stav.krabice[po.y][po.x]){
      continue;
    }
    if(!vZone(po.x,po.x))
      continue;

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


struct hrac{
    int x, y, vzd;};

bool porovnajhracov(hrac hrac1, hrac hrac2){
    return hrac1.vzd < hrac2.vzd;
}

int dostrel(int index){
    return kPrototypyZbrani[index].zakladna_sila/kPrototypyZbrani[index].pokles_sily;
}

vector<hrac> spocitatVzdalenostOdHraca(){
  const int MX = 1e9;
  queue<pair<Pozicia, int>> fronta;
  vector<vector<bool>> videno(mapa.h, vector<bool>(mapa.w, false));
  vector<vector<int>> vzdalenostOdHraca(mapa.h, vector<int>(mapa.w, MX));
  fronta.push({ja.pozicia, 0});

  while(!fronta.empty()){
    auto el = fronta.front(); fronta.pop();
    Pozicia po = el.first;
    int delka = el.second;
    if(mapa.teren[po.y][po.x] == Policko::STENA){
      continue;
    }
    if(vzdalenostOdHraca[po.y][po.x] != MX)
      continue;

    vzdalenostOdHraca[po.y][po.x] = delka;

    for(int i = 0; i < 4; i++){
      if(vMape(po.x + kDx[i], po.y + kDy[i])){
        fronta.push({Pozicia(po.x + kDx[i], po.y + kDy[i]), delka + 1});
      }
    }

  }
  cerr << "Pred " << endl;
  vector<hrac> hraci;
  hrac x;
  cerr << stav.hraci.size() << " velikost "<< endl;
  for(int i=0; i<stav.hraci.size(); i++){
    if(stav.hraci[i].pozicia == Pozicia(-1, -1))
      continue;

    x.x=stav.hraci[i].pozicia.x;
    x.y=stav.hraci[i].pozicia.y;
    x.vzd=vzdalenostOdHraca[stav.hraci[i].pozicia.y][stav.hraci[i].pozicia.x];
    hraci.push_back(x);
  }
  cerr << "Pred sortem "<< endl;
  sort(hraci.begin(),hraci.end(),porovnajhracov);
  cerr << "Konec " << endl;
  return hraci;
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
    if(mamZbran(typ))
      return 0;
    if(kPriorita[0][ja.zbrane[ja.aktualna_zbran]] < kPriorita[0][typ] ||
        kPriorita[0][ja.zbrane[(ja.aktualna_zbran+1)%2]] < kPriorita[0][typ] )
      return kPriorita[0][typ];
    else
      return 0;
  }else if(trieda == TriedaItemu::VESTA){
    return kPriorita[1][typ] * horsiVesta(typ);
  }else if(trieda == TriedaItemu::SCOPE){
    return kPriorita[2][typ] * horsiScope(typ);
  }else{
    return kPriorita[3][typ];
  }
}

// Funkce urcena jen pro vymenu veci - zbrani
double absolutniHodnotaZbrane(int typ){
  return kPriorita[0][typ];
}

inline double prioritaProItem(Item item){
  return prioritaProItem(item.trieda, item.typ);
}

Pozicia add(Pozicia pozice, int smer){
  return Pozicia(pozice.x + kDx[smer], pozice.y + kDy[smer]);
}

Pozicia add(Pozicia pozice, int smer, int skalovani){
  return Pozicia(pozice.x + kDx[smer] * skalovani, pozice.y + kDy[smer] * skalovani);
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

bool spravnySmer(Pozicia pozice){
  if(ja.smer == 0 && (abs(ja.pozicia.x - pozice.x) <= abs(ja.pozicia.y - pozice.y)) &&
      pozice.y - ja.pozicia.y < 0){
        return true;
  }else if(ja.smer == 2 && (abs(ja.pozicia.x - pozice.x) <= abs(ja.pozicia.y - pozice.y)) &&
      pozice.y - ja.pozicia.y > 0){
        return true;
  }else if(ja.smer == 1 && (abs(ja.pozicia.x - pozice.x) >= abs(ja.pozicia.y - pozice.y)) &&
      pozice.x - ja.pozicia.x > 0){
        return true;
  }else if(ja.smer == 3 && (abs(ja.pozicia.x - pozice.x) >= abs(ja.pozicia.y - pozice.y)) &&
      pozice.x - ja.pozicia.x < 0){
        return true;
  }
  return false;
}

// Najde posledni pohyb, jak se dostat nejrychleji k mistu
// POZOR: na pohyb hrace se tato hodnota musi invertovat
// - pokud jsem se dostal na posledni policko, ze jsem sel dolu, tak spravna cesta vede nahoru
int najdiPosledniPohyb(vector<vector<int>>& vzdalenosti, Pozicia pozice){
  int lastPohyb = -1;
  int vzdalenost = vzdalenosti[pozice.y][pozice.x];
  cerr << "Moje pozice " << ja.pozicia.x << " " << ja.pozicia.y << endl;
  while(!(pozice == ja.pozicia)){
    //cerr << "Pozice pohled " << pozice.x << " " << pozice.y << endl;
    for(int i = 0; i < 4; i++){
      Pozicia novaPozicia = add(pozice, i);
      if(!vMape(novaPozicia.x, novaPozicia.y))
        continue;
      int novaVzd = vzdalenosti[novaPozicia.y][novaPozicia.x];
      //cerr << "vzd " << vzdalenost << " " << novaVzd << endl;
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

vector<vector<int>> spocitatVzdalenostBezZony(){
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
    /*if(stav.krabice[po.y][po.x]){
      continue;
    }*/

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

Prikaz jidDoStredu(){
  auto vzdalenosti = spocitatVzdalenostBezZony();
  int midX = (stav.zona_x1 + stav.zona_x2) / 2;
  int midY = (stav.zona_y1 + stav.zona_y2) / 2;
  Pozicia def = Pozicia(midX, midY);
  int kolikrat = 1;
  cerr << "lastPart" << endl;
  while(vzdalenosti[def.y][def.x] == 1e9){
    for(int i = 0; i < 4; i++){
      Pozicia newPoz = add(def, i, kolikrat);
      if(vzdalenosti[newPoz.y][newPoz.x] != 1e9){
        def = newPoz;
        break;
      }
    }
    kolikrat++;
  }
  cerr << def.x << " " << def.y << endl;
  int lastPohyb = najdiPosledniPohyb(vzdalenosti, def);
  cerr << lastPohyb << " lastP" << endl;
  lastPohyb = invertovatSmer(lastPohyb);
  Prikaz ret = otocSeNeboPohni(ja.smer, lastPohyb);
  cerr << "konecna cast " << endl;
  if(ret.typ == TypPrikazu::CHOD){
    cerr << "Pohni se" << endl;
    Pozicia predemnou = add(ja.pozicia, lastPohyb);
    cerr << "last " << predemnou.x << " " << predemnou.y << endl;
    if(stav.krabice[predemnou.y][predemnou.x]){
      return prikazVystrel(predemnou);
    }
  }

  return ret;
}

// main() zavola tuto funkciu, ked chce vediet, ake prikazy chceme vykonat
Prikaz zistiTah() {
  cerr << vZone(ja.pozicia.x, ja.pozicia.y) << endl;
  if(!vZone(ja.pozicia.x, ja.pozicia.y)){
    return jidDoStredu();
  }else{
    auto vzdalenosti = spocitatVzdalenost();

    /*for(int y = 0; y < mapa.h; y++){
      for(int x = 0; x < mapa.w; x++){
        cerr << vzdalenosti[y][x] <<" ";
      }
      cerr << endl;
    }*/

    // TODO: Pokud stoji blizko nepritel a ja mam zbran na dalku - strilet

    //
    // Strileni na hrace
    //

    auto hraci = spocitatVzdalenostOdHraca();
    int obet;
    int dostrel1=max(dostrel(ja.zbrane[0]),dostrel(ja.zbrane[1]));
    if(dostrel1>19){dostrel1-=2;}
    if(dostrel1>30){dostrel1-=3;}
    for(obet=1; obet<hraci.size(); obet++){
        cerr << "Vzdalenost hrace" << obet <<" " << hraci[obet].vzd << endl; 
        if(hraci[obet].vzd<=dostrel1 && !zavadziaStena(hraci[obet].x,hraci[obet].y)){break;}
    }
    cerr << obet << " " << hraci.size() << " hraci" << endl;
    if(obet < hraci.size()){
      cerr << "Obet " << obet << " " << hraci[obet].x << " " << hraci[obet].y << endl; 
    
      int A=max(max(hraci[obet].x-ja.pozicia.x,ja.pozicia.x-hraci[obet].x),max(hraci[obet].y-ja.pozicia.y,ja.pozicia.y-hraci[obet].y));

      if(!((ja.smer == 0 && ja.pozicia.y-hraci[obet].y==A) || (ja.smer == 1 && hraci[obet].x-ja.pozicia.x==A)||
      (ja.smer == 2 && hraci[obet].y-ja.pozicia.y==A) || (ja.smer == 3 && ja.pozicia.x-hraci[obet].x==A))){
          if(hraci[obet].y-ja.pozicia.y==A){       // Juh
              return prikazOtocSa(2);}
          else{if(ja.pozicia.y-hraci[obet].y==A){  // Sever
              return prikazOtocSa(0);}
          else{if(hraci[obet].x-ja.pozicia.x==A){  // Vychod
              return prikazOtocSa(1);}
          else{if(ja.pozicia.x-hraci[obet].x==A){  // Zapad
              return prikazOtocSa(3);}}}}
      }
      if(hraci[obet].vzd==1){
          if(ja.zbrane[0]==3||ja.zbrane[1]==3){if(ja.zbrane[ja.aktualna_zbran]!=3){return prikazZmenZbran();}}
          else if(ja.zbrane[0]==4||ja.zbrane[1]==4){if(ja.zbrane[ja.aktualna_zbran]!=4){return prikazZmenZbran();}}
          else if(ja.zbrane[0]==1||ja.zbrane[1]==1){if(ja.zbrane[ja.aktualna_zbran]!=1){return prikazZmenZbran();}}
          else{if(ja.zbrane[ja.aktualna_zbran]!=0){return prikazZmenZbran();}}} //paste
      if(hraci[obet].vzd==2){
          if(ja.zbrane[0]==4||ja.zbrane[1]==4){if(ja.zbrane[ja.aktualna_zbran]!=4){return prikazZmenZbran();}}
          else if(ja.zbrane[0]==1||ja.zbrane[1]==1){if(ja.zbrane[ja.aktualna_zbran]!=1){return prikazZmenZbran();}}
          else{if(ja.zbrane[ja.aktualna_zbran]!=2){return prikazZmenZbran();}}} //sniperka
      if(hraci[obet].vzd==3){
          if(ja.zbrane[0]==1||ja.zbrane[1]==1){if(ja.zbrane[ja.aktualna_zbran]!=1){return prikazZmenZbran();}}
          else if(ja.zbrane[0]==2||ja.zbrane[1]==2){if(ja.zbrane[ja.aktualna_zbran]!=2){return prikazZmenZbran();}}
          else{if(ja.zbrane[ja.aktualna_zbran]!=4){return prikazZmenZbran();}}} //brokovnica
      if(hraci[obet].vzd>3 && hraci[obet].vzd<11){
          if(ja.zbrane[0]==1||ja.zbrane[1]==1){if(ja.zbrane[ja.aktualna_zbran]!=1){return prikazZmenZbran();}}
          else{if(ja.zbrane[ja.aktualna_zbran]!=2){return prikazZmenZbran();}}} //sniperka
      if(hraci[obet].vzd>10){
          if(ja.zbrane[0]==2||ja.zbrane[1]==2){if(ja.zbrane[ja.aktualna_zbran]!=2){return prikazZmenZbran();}}
          else{if(ja.zbrane[ja.aktualna_zbran]!=1){return prikazZmenZbran();}}} //samopal

      return prikazVystrel(hraci[obet].x,hraci[obet].y);
    }

    if(ja.zivoty < 80)
      return prikazLiecSa();

    //
    // Sebrani predmetu
    //
    bool prehodNasazenouZbran = false;
    int index = -1; double indexPriority = 0;
    for(int i = 0; i < stav.itemy[ja.pozicia.y][ja.pozicia.x].size(); i++){
      Item item = stav.itemy[ja.pozicia.y][ja.pozicia.x][i];
      if(prioritaProItem(item) > indexPriority){
        cerr << "Zkousim brat predmet " << endl;
        if(item.trieda == TriedaItemu::SCOPE)
          cerr << "Mam scope " << ja.scope << " lezici vesta " << item.typ << endl;
        if(item.trieda == TriedaItemu::VESTA)
          cerr << "Mam vestu " << ja.vesta << " lezici vesta " << item.typ << endl;
        
        if(item.trieda == TriedaItemu::SCOPE && ja.scope >= item.typ)
          continue;
        if(item.trieda == TriedaItemu::VESTA && ja.vesta >= item.typ)
          continue;
        
        if(item.trieda == TriedaItemu::ZBRAN){
          cerr << "Moje zbrane: " << ja.zbrane[0] << ", " << ja.zbrane[1] << endl;
          cerr << "Lezici zbran " << item.typ << endl;
        }

        if(item.trieda == TriedaItemu::ZBRAN && 
          absolutniHodnotaZbrane(ja.zbrane[ja.aktualna_zbran]) < absolutniHodnotaZbrane(item.typ)){
          // vezmu si lepsi zbran
          prehodNasazenouZbran = false;
        }else if(item.trieda == TriedaItemu::ZBRAN && 
          absolutniHodnotaZbrane(ja.zbrane[(ja.aktualna_zbran+1)%2]) < absolutniHodnotaZbrane(item.typ)){
            // vezmu si lepsi zbran, ale nejdrive musim prohodit aktivni zbran
            prehodNasazenouZbran = true;
        }else{
          // neco vezmu drive, co ma vetsi prioritu
          prehodNasazenouZbran = false;
        }

        index = i;
        indexPriority = prioritaProItem(item);
      
      }  
    }
    if(prehodNasazenouZbran)
      return prikazZmenZbran();
    if(index != -1){
      return prikazZdvihni(index);
    }


    //
    // Najiti predmetu
    //
    cerr << "Dobre" << endl;
    Pozicia pozice = Pozicia(-1, -1);
    double bestP = 0;
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
    if(bestP != 0){
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
    int nejblize = 1e9-1;
    for(int w = max(0, ja.pozicia.x - dohled); w < min(mapa.w, ja.pozicia.x + dohled); w++){
      for(int h = max(0, ja.pozicia.y - dohled); h < min(mapa.w, ja.pozicia.y + dohled); h++){
        if(vZone(w, h) && stav.krabice[h][w] && nejblize > vzdalenosti[h][w]){
          pozice = Pozicia(w, h);
          nejblize = vzdalenosti[h][w];
        }
      }
    }

    cerr << "Nalezena krabice sour " << pozice.x << " " << pozice.y << endl;
    if(pozice.x != -1){
      cerr << "Vzd " << vzdalenosti[pozice.y][pozice.x] << endl;
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
          cerr << "Pred ke krabici 2> " << endl;
          int smer = najdiPosledniPohyb(vzdalenosti, pozice);
          cerr << "Smer ke krabici 2: " << smer << endl; 
          Prikaz p = otocSeNeboPohni(ja.pozicia, invertovatSmer(smer));
          if(spravnySmer(pozice)){
            vector<Zbran> zb= vector<Zbran>();
            zb.push_back(kPrototypyZbrani[ja.zbrane[0]]);
            zb.push_back(kPrototypyZbrani[ja.zbrane[1]]);
            if(!zavadziaStena(pozice.x, pozice.y)){
              if(zb[0].zakladna_sila / zb[0].pokles_sily >= vzdalenosti[pozice.y][pozice.x] &&
                zb[1].zakladna_sila / zb[1].pokles_sily >= vzdalenosti[pozice.y][pozice.x]){
                  if(zb[(ja.aktualna_zbran+1)%2].cas_medzi_strelami >= zb[ja.aktualna_zbran].cas_medzi_strelami + kCasZmenyZbrane)
                    return prikazZmenZbran();
              }
              if(zb[ja.aktualna_zbran].zakladna_sila / zb[ja.aktualna_zbran].pokles_sily >= vzdalenosti[pozice.y][pozice.x]){
                return prikazVystrel(pozice.x, pozice.y);
              }else if(zb[(ja.aktualna_zbran + 1) %2].zakladna_sila / zb[(ja.aktualna_zbran + 1) %2].pokles_sily >= vzdalenosti[pozice.y][pozice.x] &&
                zb[(ja.aktualna_zbran + 1) %2].cas_medzi_strelami < vzdalenosti[pozice.y][pozice.x] * kCasPohybu[0]){
                return prikazZmenZbran();
              }
            }
          }
          return p;
      }
    }

    if(ja.zivoty < 60)
      return prikazLiecSa();

    cerr << "Zadny cil" << endl;
    Prikaz ret =  jidDoStredu();
    cerr << ret.typ << " " << ret.parametre[0] << " " << ret.parametre[1] << endl;
    cerr << "Konec tahu" << endl;
    return ret;
    //
    // Pokud se dostanu sem - strilim by default
    //
    //return prikazVystrel(ja.pozicia.x + kDx[ja.smer], ja.pozicia.y + kDy[ja.smer]);
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

