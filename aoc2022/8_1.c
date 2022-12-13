#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define WIDTH 99
#define HEIGHT 99
static uint8_t input[] =
    "20221001031030212132221042320122000031402424243221142543442223013041130032"
    "13243022230113112110201201101101013103223201010340331243033430311434351223"
    "51113353455142421341123420341013300312303121102011022222013123323313404200"
    "43024302002223255543224433434444454212213413013341304434223023313002202111"
    "21121112002122212041001210222441214534124141411451545515124455254214352321"
    "12124343223122023223002202010223032133232313122112411243113131542422453412"
    "41332533414142413212534100412340334203111300032102112020331001300111301423"
    "21323533223412351345234241313535541354332554244144413234100431130102100210"
    "12001321033020303133033413032514323122112454135451341114535233523552111335"
    "31530341144241310333231303010223300110001321013040255155554152111333545215"
    "42226265214515451514315435315411203323114011232330020000122202231130313131"
    "23321342142243351454565666666364326432622135251241345515402410441240003203"
    "23132312011021321221214335353454422324234622555663352632523336544251531545"
    "44521355214220310324301312200102203103312033302251424313255311265536653365"
    "62254653354425352265653122233143242323103214302312331123101224243131422134"
    "41423342333454442563352464624323336266453566366455413535155532420023321103"
    "23021003333031124012433315231253332254244542226624425436634565564325655642"
    "35231542334341022204140131010222320010441120114425514454524226565533325532"
    "62343226536263625644624466333351152332540030434013300200113413102314045351"
    "43412543222625445433446265644335443263423362432525643555555141534110413223"
    "21023303032023104304131222142562326266256323662554334643363347342242532462"
    "53426335242542243313313440133202034031021134525424245444426545433256553763"
    "66765543744765363574642423343645253322314114423413043330104242442144511514"
    "34435346345243633344375647763655474336663454733535632266334124355143530400"
    "22200202010423423255414145162232632226426364773544765436745734444446564345"
    "36343363652312145251244330224442042011300043311233252626342654243635347543"
    "56365344543544653337444375665555552553542414222233041401231313304412112545"
    "14354344662657776667444457367463335533677773753554353654663346241442151220"
    "42344000333414202453413414443526552366567446665364737637666746375737374557"
    "77566262423256254341313453322144440314332142235331442343546273646373345664"
    "65744864787876777356353735767764245435442414111355443430100202142535224453"
    "62432535654664553467366776748844486778658684767656566747425343634624511334"
    "33430431312244242113113354564554337535376574767786648687855878644777578636"
    "67764557442553223531531134324241123312224515514456525555633355753676484758"
    "76856674678746884777657477367335664633442326653534342420410222204525134555"
    "55322662755373537664785775876754758887458644778857853773647663236626545124"
    "42242432002223033233124252564525557573344555445476568876865484548567457555"
    "67567643775372664333425131114113341203411115221445526632663544776448458487"
    "48677677644558776677456646677433474757754324325455254224404334433112533124"
    "46636263644645775567457564776787868975799765465555744746773464664362366332"
    "42411233410440445112536354654465343544474767464448848899898575977978667877"
    "87888747443665545532456524224131143342122313235634465626773753655655586857"
    "76799856865767569789968478788865556466356443266344434512454130024141554324"
    "22226743764547585787488599658969857896598787957787584685788564746556223532"
    "23553421340432555555434536525465544664745745888686778996656856565599757587"
    "74675574578453365464423362242551543400415514246322523543457557777776476659"
    "66655696578797957658575775964674586444476344535643635325314333151143451323"
    "32442746733438647847847577597775567596889586878967997655654786546753744554"
    "45631343421332355431113362626637534565574586548578856987888767789775699858"
    "95765566567846776637466326633325321513151441322325535375333545546544645988"
    "75898876799867967877875556975854576455736577735632354543155134222334432553"
    "63453366673347768755698557999986777699999789866887867886785846644545375365"
    "24352555523533413344525244654355537364856555575685596997889779966676699996"
    "67955587886767744364375524653661522332413445246252544336676458575775678988"
    "98858668777697866988878988986887586646747743547446465353522323311314344622"
    "63653376464488564754675756687789667979866667899679878669895445787543336674"
    "36355422314512323412424644366374665378846687555957879987766978769867869789"
    "98686769655445485465676375662532524114532342111554552564363354785884788999"
    "96977766689796789767698979998996598755586847775654463235443242514323343344"
    "44542573375354656474858797778897676698987797799688968867795956877875445655"
    "54526442434523231342152662546465474578466755565587766786688999797799897989"
    "99689587669686888847457677453423365222241222112642645627635444884846899759"
    "89588788879988798787779667786859558567878485674347334443246223344255242155"
    "63662244333548557865856868587877767799978777889876768698755878887684656643"
    "53733643664245334133441562564336535747686547888787779786999698789987988878"
    "69869888598888664446534547775236363642425342124563455467737457658485456878"
    "99898998897989999889778798786687569866985657585346367365544453434123255126"
    "62324266743346884685796887698989799788977887888997988686999755754444747377"
    "64445664236253252525112465464323755636656578776889996878689879787788998889"
    "98797898695769585566873637776625232335325533533544555465364456334455468585"
    "89879788787889879778797789999676678695674888477634737675655624435535453215"
    "24224436644545685678886975585679878668999989988977889988665666777667584866"
    "43736442335423332311142326634244374654366755447687557566697766888777879879"
    "77678688955689594865646575544736324456553445242125523553644773473655564886"
    "88656986668766897897789886896688988559599878748555366373562653454121131154"
    "33543342234563464565545756855688687678778977997998786666996799757857775474"
    "55774626462264453142352524523352345643676784768876879958978986686787787866"
    "87699867598786667847747567563645652263343425223253143456366656767765746865"
    "55756756666676786978998798998688755795699868547463763736635232624354202232"
    "55553344565673537588474544997758656977668866666787998978669888588688874475"
    "54444762544422112415343144416534333377537365468647569967688667888678877876"
    "69868896769698795848847457653355323646422251504145541355363664547435656685"
    "77777979559677977678966968886985585697996578857736676636422633334322253142"
    "32125326436446553663846575645898685777676969798987998978856859686786577445"
    "37755374444324543224332124322354645536436774347475877765957865877799766889"
    "77889895666857888886844765633454233445544432314151252544224425276665736457"
    "87574596577688978657789966586986685976985678486637347667436525212454543414"
    "23513554563336366635643875746766985896985768775589857885767685868864454775"
    "37473454323663125515414401142423443455463366475657574545868559997576678667"
    "76678779875958488554786743465373523322335324440122344413244444335537455544"
    "74846755457878857799787997969695869698475585758743355633664324542441451140"
    "41525323532563263735734567648787884866657867866869898857888587878466747443"
    "54647343463555324212243430052142415544522224767753334887658877858987557899"
    "58758585877448646768865675564765656245221544434134021445114265535433533466"
    "74674847486846479665777567977968765646847454867744567446244423325555442142"
    "02003423413136555225554443743364668647485745965556658974868564455687766677"
    "64633342456324422531241312424425223254663235443554747757545445847488878564"
    "88555888685476567474346446445626663564255135522341100043432252545522322453"
    "33753757468577454458667486664868686776847774656744433232645435354515511001"
    "11411024352331224664663366765744655788776457787544467458644566885564645546"
    "46734263552461212414202421440040221152323353252335444574633545565775586868"
    "66645777668486456344336776563552654224535341204000421204313533552465364335"
    "66565775637747868748574756846445758657843353366537754433443632411534511024"
    "23102131024555335462665356243453343763636848587754676576855557473536436444"
    "72546632522545411333203111114430325255224412546642432664757677554453367648"
    "66747486656336665354767354564355652544524122101404410120202412431114133224"
    "66253444577367637737435676763644346543354376676733336333345333211411444013"
    "43022314412343141552545426345523734644465534437755555546366776455573555736"
    "34656655452343144531204114131010232011511341325566262246366664577447576447"
    "47635763475534645766575446552322652134123321133212401034214431242213143415"
    "26656425352447567466347367455546655457665443752365626234642233144534440401"
    "23201224210242143234121241344332626646243535333444375755463677657535256454"
    "43643263453422332114000211322223333200140224341543324243233232223435653755"
    "55437333746365455224462465255645335454253531041200430203301220423445445245"
    "34333356652344424565373466435537536635654322542545356655111553542221440424"
    "10023023244430311223314413312563353442242536365335567564454346433545325644"
    "54235413132113211343002133211311312211414143323513134241534622563433242365"
    "62665642252634522335454636443531545155110102321141213022211034340103204355"
    "34251113122446543326534646664354233424533226634242431532333221130320232020"
    "20223323230200411432130355525221321433355242633433225442453325522522622364"
    "25331153333435403110022201001331022231214343311424243135551531555464353432"
    "43424456566664363532234442314354532414002133213301300103330030323133040332"
    "43534455111112214234262552344526635435332246231323535122413441024143242102"
    "30211011302320230341332332405411532113534222335442556625653655625531211542"
    "43533511515330102214132103301132010021011320130104112145253535342411134232"
    "51253436544643553152152432455455322331331310031222312310010000023323114442"
    "20423321414522245352325454311121311252544454545135424112232033134402333030"
    "10100000202212013012040432413402120252314515545125453555444543553342355443"
    "31435121232034423404333303220120211200133121002321411044312343241534545415"
    "43553521455152252353452134251043201243321102013222222311000101011133302123"
    "21140334321230142431434211314452112325445215545345420304343133410200333122"
    "33321020110102200233103010320200140144034422131352541143533213514525422131"
    "220112004103004000011012120121220";
static bool visible[HEIGHT][WIDTH];

int main(void) {
  // Raytrace each row from the left
  for (uint64_t y = 0; y < HEIGHT; y++) {
    uint8_t max_row = 0;
    for (uint64_t x = 0; x < WIDTH; x++) {
      const uint8_t cell = input[y * WIDTH + x];
      if (cell > max_row) {
        visible[y][x] = true;
        max_row = cell;
      }
    }
  }
  // Raytrace each row from the right
  for (uint64_t y = 0; y < HEIGHT; y++) {
    uint8_t max_row = 0;
    for (uint64_t j = 0; j < WIDTH; j++) {
      const uint64_t x = WIDTH - 1 - j;
      const uint8_t cell = input[y * WIDTH + x];
      if (cell > max_row) {
        visible[y][x] = true;
        max_row = cell;
      }
    }
  }
  // Raytrace each column from the top
  for (uint64_t x = 0; x < WIDTH; x++) {
    uint8_t max_col = 0;
    for (uint64_t y = 0; y < HEIGHT; y++) {
      const uint8_t cell = input[y * WIDTH + x];
      if (cell > max_col) {
        visible[y][x] = true;
        max_col = cell;
      }
    }
  }
  // Raytrace each column from the bottom
  for (uint64_t x = 0; x < WIDTH; x++) {
    uint8_t max_row = 0;
    for (uint64_t j = 0; j < HEIGHT; j++) {
      const uint64_t y = HEIGHT - 1 - j;
      const uint8_t cell = input[y * WIDTH + x];
      if (cell > max_row) {
        visible[y][x] = true;
        max_row = cell;
      }
    }
  }
  uint64_t sum = 0;
  for (uint64_t i = 0; i < WIDTH * HEIGHT; i++)
    sum += ((bool *)visible)[i];

  printf("%llu\n", sum);
  return 0;
}
