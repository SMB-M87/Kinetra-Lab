We werken aan **Kinetra-Lab**, met de engine als submodule in `external/Kinetra`. De applicaties zijn **i4sim, Demo en Dotio**. Communiceer in het Nederlands en volg de toepasselijke `AGENTS.md`-instructies. Laat wijzigingen ongecommit; ik beheer Git zelf.

**Mijn doel**

Ik wil geloofwaardige beweging, steering en collision avoidance voor meerdere agents. Uiteindelijk moeten i4sim, Demo en Dotio dezelfde herbruikbare mechanismen gebruiken, met applicatiespecifieke verkeersregels waar nodig.

Demo dient als demonstratie van drukke multi-agent collision avoidance, met verschillende vormen, waaronder driehoeken, en agents die werkelijk rondwanderen. Debug Mode en alle debuglagen moeten standaard aan staan in Demo en i4sim. Eerdere freezes van Demo zijn aangepakt; behoud de bijbehorende oplossingen.

**Wat al is opgebouwd**

- Gedeelde geometrie-, contact-, steering- en avoidancefunctionaliteit in Kinetra.
- i4sim heeft de profielen Improved, Original Basic, experimental RVO en Reciprocal. Improved blijft standaard.
- Reciprocal gebruikt gedeelde avoidance met onveranderlijke snapshots van de buren en rechthoekige mover-footprints.
- Optionele i4sim-verkeersregels voor parking, kruispunten en smalle corridors.
- Corridorherstel waarbij een mover tijdelijk terugtrekt om een ander te laten passeren.
- Debugweergaven en regressietests voor deze functies.

**Meest recente implementatie: predictive passing in i4sim**

Probleem: movers kunnen elkaar tegemoetkomen en vastlopen doordat hun routes elkaar blijven tegenwerken.

De nieuwe optionele functie:
- Voorspelt conflicten tussen ongeveer tegengesteld bewegende movers.
- Laat twee movers gezamenlijk tijdelijke passeerbanen kiezen.
- Laat eerst beide movers hun baan innemen en vervolgens passeren.
- Behoudt hun oorspronkelijke bestemmingen; na het passeren worden de routes opnieuw gepland.
- Controleert de manoeuvre op muren, andere mover-footprints, gedetecteerde verkeerszones en bestaande passeerafspraken.
- Annuleert afspraken bij nieuwe opdrachten, blokkering, uitschakeling of langdurig uitblijven van voortgang.
- Verplaatst movers niet door teleportatie.

Inschakelen via **DEBUG → View → PREDICTIVE PASS**. Ondersteund door Improved en Reciprocal; Original en RVO behouden hun bestaande gedrag. Mintgroene pijlen tonen tijdelijke doelen en `PASS` toont het aantal actieve paren.

De functie staat **standaard uit**, omdat fabrieksmetingen gemengde resultaten geven.

**Relevante bestanden**

- `labs/i4sim/src/predictive_passing.cpp`
- `labs/i4sim/src/simulation.cpp`
- `labs/i4sim/include/simulation.h`
- `labs/i4sim/src/session.cpp`
- `labs/i4sim/tests/simulation_tests.cpp`
- `labs/i4sim/tests/presentation_tests.cpp`
- `labs/i4sim/tests/predictive-passing.json`
- `labs/i4sim/tests/motion_benchmark.cpp`
- `docs/factory-simulation-architecture.md`
- `labs/i4sim/tests/README.md`

Controleer eerst de actuele werkboom en documentatie; de commitstatus kan inmiddels veranderd zijn.

**Verificatie en bevindingen**

- Gerichte tests slagen voor acht ontmoetingsrichtingen, gemengde vierkante/langwerpige footprints, determinisme, behouden doelen, begrensde acceleratie, annulering en uitwijken langs een wand.
- Deze gecontroleerde ontmoetingen eindigen zonder contacten.
- De normale rootbuild en volledige testsuite slaagden: **14/14 tests**, inclusief graphics en Demo-duurtests.
- Launcher, Demo, Dotio en i4sim zijn met een graphicscontext gestart.
- Een gerichte UBSan-controle gaf geen meldingen.
- Bij benchmarks van 50.000 ticks bleef Reciprocal in alle zes onderzochte runs contactvrij. Op de grote layout steeg het aantal afgeronde producten van 95/93 naar 99/103.
- Improved gaf gemengde resultaten en soms extra contacten. De functie is dus nog geen algemene garantie tegen botsingen of deadlocks.
- De huidige controles gebruiken snapshots; ze beschermen een passeerbaan niet volledig tegen een derde mover die later binnenkomt.

**Voorgestelde volgende stap**

Maak predictive passing eerst onder druk betrouwbaar in i4sim, met Reciprocal als uitgangspunt:

1. **Maak probleemsituaties reproduceerbaar.** Registreer deelnemers, gekozen banen, fasewissels en redenen voor annulering. Leg seed en tick vast bij contact, zodat een probleem gericht kan worden herhaald.
2. **Voeg gerichte druktescenario’s toe.** Test drie tegemoetkomende movers, een derde mover die een passeerbaan binnenkomt, meerdere gelijktijdige passeerafspraken en beperkte uitwijkruimte.
3. **Onderzoek de extra contacten bij Improved.** Stel de oorzaak vast voordat je parameters of gedrag aanpast.
4. **Bewaak actieve manoeuvres.** Controleer tijdens het passeren opnieuw of de manoeuvre uitvoerbaar blijft. Ontwerp gecontroleerd wachten of afbreken met vaste voorrang, zodat movers niet voortdurend van beslissing wisselen. Bewaak daarbij ook remweg en acceleratiegrenzen.
5. **Meet veiligheid én doorstroming.** Vergelijk identieke seeds, layouts en instellingen met de functie aan en uit. Controleer contacten, voltooide taken, wachttijd en vastgelopen movers.
6. **Deel pas daarna het bewezen mechanisme.** Verplaats het herbruikbare deel naar Kinetra en gebruik dezelfde scenario’s in Demo en Dotio. Houd fabrieksbeleid in i4sim.

Begin met inspectie van de actuele code, tests en instructies. Voer vervolgens de eerstvolgende concrete verbetering uit, met passende regressietests en de vereiste integratieverificatie.
