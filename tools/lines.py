# -*- coding: utf-8 -*-
"""All presenter lines for Koffer Koorts / Case Fever.

Every group holds a list of variants; each variant is a (dutch, english) pair.
The game picks a random variant per group and never repeats the previous one.
"""
from num2words import num2words

AMOUNTS_CENTS = [1, 100, 500, 1000, 2500, 5000, 7500, 10000, 20000, 30000, 40000,
                 50000, 75000, 100000, 500000, 1000000, 2500000, 5000000, 7500000,
                 10000000, 20000000, 30000000, 40000000, 50000000, 75000000, 100000000]

PLAYER_COLORS = [("Rood", "Red"), ("Blauw", "Blue"), ("Geel", "Yellow"), ("Groen", "Green")]


def nl_num(n):
    return num2words(n, lang="nl")


def en_num(n):
    return num2words(n, lang="en")


def offer_values():
    """All euro values a banker offer can take (rounded to 2 significant digits)."""
    vals = set(range(1, 100))
    for mag in (100, 1000, 10000, 100000):
        step = mag // 10
        for v in range(mag, mag * 10, step):
            vals.add(v)
    vals.add(1000000)
    return sorted(vals)


def amount_words(cents):
    if cents == 1:
        return ("één cent", "one cent")
    e = cents // 100
    nl = nl_num(e) + " euro"
    en = en_num(e) + (" euro" if e == 1 else " euros")
    return (nl, en)


def amount_subtitle(cents):
    if cents == 1:
        return ("€ 0,01", "€0.01")
    e = cents // 100
    return ("€ " + f"{e:,}".replace(",", "."), "€" + f"{e:,}")


G = {}  # group -> list of (nl, en)


def add(group, *variants):
    G.setdefault(group, []).extend(variants)


# ---------------------------------------------------------------- intro / menu
add("title",
    ("Koffer Koorts!", "Case Fever!"),
    ("Welkom bij Koffer Koorts!", "Welcome to Case Fever!"),
    ("Daar is de show weer! Koffer Koorts!", "The show is back! Case Fever!"))

add("welcome",
    ("Goedenavond, goedenavond! Welkom bij Koffer Koorts, de show waar één koffer je leven kan veranderen!",
     "Good evening, good evening! Welcome to Case Fever, the show where one case can change your life!"),
    ("Daar ben je eindelijk! Het publiek zit klaar, de bankier zit klaar, en de koffers staan te glimmen. Welkom!",
     "There you are at last! The audience is ready, the banker is ready, and the cases are gleaming. Welcome!"),
    ("Welkom, welkom! Vanavond staat er één miljoen euro op het spel. Ben je er klaar voor?",
     "Welcome, welcome! Tonight there is one million euros at stake. Are you ready?"),
    ("Dames en heren, applaus voor onze kandidaat! Welkom bij Koffer Koorts!",
     "Ladies and gentlemen, give it up for our contestant! Welcome to Case Fever!"),
    ("Goedenavond! Ik heb er zin in, het publiek heeft er zin in, en de bankier... die heeft vooral zin in jouw geld. Welkom!",
     "Good evening! I'm excited, the audience is excited, and the banker... well, the banker is mostly excited about your money. Welcome!"),
    ("Welkom bij Koffer Koorts! Zesentwintig koffers, één bankier, en jij. Laten we beginnen!",
     "Welcome to Case Fever! Twenty six cases, one banker, and you. Let's get started!"))

add("rules1", ("Het is heel simpel. Er staan zesentwintig koffers, en in elke koffer zit een geldbedrag. Van één cent, tot één miljoen euro.",
               "It's very simple. There are twenty six cases, and each case holds an amount of money. From one cent, all the way up to one million euros."))
add("rules2", ("Eerst kies je één koffer voor jezelf. Die blijft dicht, tot het allerlaatste moment.",
               "First you choose one case for yourself. It stays closed until the very end."))
add("rules3", ("Daarna open je steeds andere koffers. Zit er een laag bedrag in? Mooi! Dan zit dat niet in jouw koffer.",
               "Then you open the other cases, one by one. A low amount inside? Great! That means it's not in your case."))
add("rules4", ("Na elke ronde belt de bankier met een bod. Pak je het geld, of speel je door? Jij beslist!",
               "After every round the banker calls with an offer. Do you take the money, or do you play on? You decide!"))
add("rules5", ("Wijs met je controller naar het scherm, of gebruik de joystick. Druk op de trekker om te kiezen. Met L of R zet je de aanwijzer terug in het midden.",
               "Point your controller at the screen, or use the stick. Pull the trigger to choose. Press L or R to bring the pointer back to the middle."))

add("menu_hint",
    ("Kies maar wat je wilt doen.", "Go ahead and choose what you want to do."),
    ("Solo spelen, of met z'n allen? Kies maar!", "Play solo, or all together? Your choice!"))

add("settings",
    ("Hier kun je alles instellen zoals jij het wilt.", "Here you can set everything up just the way you like it."),)

add("lang_set", ("Ik spreek vanaf nu Nederlands.", "From now on I'll speak English."))

# ---------------------------------------------------------------- picking your own case
add("pick_own",
    ("Welke koffer wordt de jouwe? Kies maar!", "Which case will be yours? Go ahead and pick!"),
    ("Kies een koffer die je mee naar huis wilt nemen. Volg je gevoel!", "Pick a case you'd like to take home. Trust your gut!"),
    ("Zesentwintig koffers, maar er is er maar één van jou. Welke wordt het?", "Twenty six cases, but only one of them is yours. Which will it be?"),
    ("Neem je tijd. Welke koffer roept jouw naam?", "Take your time. Which case is calling your name?"),
    ("Tijd om je eigen koffer te kiezen. Geluksnummer, iemand?", "Time to choose your own case. Lucky number, anyone?"))


def per_case(group, templates):
    for n in range(1, 27):
        for nl, en in templates:
            add(f"{group}_{n}", (nl.format(n=nl_num(n).capitalize(), nn=nl_num(n)),
                                 en.format(n=en_num(n).capitalize(), nn=en_num(n))))


per_case("own", [
    ("Koffer {nn}! Een prima keuze.", "Case {nn}! A fine choice."),
    ("Je gaat voor koffer {nn}. Ik hoop dat er iets moois in zit!", "You're going with case {nn}. I hope there's something nice inside!"),
    ("Koffer nummer {nn} is van jou. Pas er goed op!", "Case number {nn} is yours. Take good care of it!"),
])

per_case("open", [
    ("Koffer {nn}, open maar!", "Case {nn}, open up!"),
    ("We openen koffer {nn}.", "Let's open case {nn}."),
    ("Koffer {nn}... laat maar zien wat je hebt!", "Case {nn}... show us what you've got!"),
    ("Oké, koffer {nn}. Spannend!", "Okay, case {nn}. Exciting!"),
])

# ---------------------------------------------------------------- rounds
for k in range(1, 7):
    if k == 1:
        add("round_1",
            ("Deze ronde open je maar één koffer.", "This round you only open one case."),
            ("Eén koffer deze ronde. Maak hem goed!", "Just one case this round. Make it count!"))
    else:
        add(f"round_{k}",
            (f"Deze ronde open je {nl_num(k)} koffers.", f"This round you open {en_num(k)} cases."),
            (f"Ronde begint! Kies {nl_num(k)} koffers om te openen.", f"New round! Pick {en_num(k)} cases to open."))

add("left_1",
    ("Nog één koffer, en dan belt de bankier.", "One more case, and then the banker calls."),
    ("Laatste koffer van deze ronde!", "Last case of this round!"),
    ("Nog eentje. Kies wijs!", "Just one more. Choose wisely!"))
for k in range(2, 6):
    add(f"left_{k}",
        (f"Nog {nl_num(k)} koffers deze ronde.", f"{en_num(k).capitalize()} more cases this round."),
        (f"Nog {nl_num(k)} te gaan.", f"{en_num(k).capitalize()} to go."))

add("suspense",
    ("En daarin zit...", "And inside is..."),
    ("Het bedrag is...", "The amount is..."),
    ("Kijk eens aan...", "Well, look at that..."),
    ("Daar gaat ie...", "Here it goes..."),
    ("Moment van de waarheid...", "Moment of truth..."),
    ("Het is...", "It's..."),
    ("Laten we kijken...", "Let's see..."))

add("react_great",
    ("Fantastisch! Precies wat we wilden zien!", "Fantastic! Exactly what we wanted to see!"),
    ("Ja! Weg met dat kleine bedrag!", "Yes! Good riddance to that small amount!"),
    ("Heerlijk! Het publiek gaat helemaal los!", "Lovely! The crowd goes wild!"),
    ("Wat een opluchting! Prachtig!", "What a relief! Beautiful!"),
    ("Perfect! De bankier zit nu te zweten.", "Perfect! The banker is sweating now."),
    ("Top! Zo wil ik het zien!", "Brilliant! That's how I like it!"))
add("react_good",
    ("Mooi zo, dat valt mee.", "Nice, that's not bad at all."),
    ("Prima! Daar kun je mee leven.", "Fine! You can live with that."),
    ("Goed gedaan, een klein bedrag minder.", "Well done, one small amount gone."),
    ("Dat is een nette opening.", "That's a tidy result."),
    ("Helemaal oké. Door!", "Totally fine. Onward!"))
add("react_meh",
    ("Hmm. Niet slecht, niet geweldig.", "Hmm. Not bad, not great."),
    ("Ach, daar valt mee te leven.", "Oh well, you can live with that."),
    ("Een middelmatig bedrag. Het spel blijft spannend.", "A middling amount. The game stays exciting."),
    ("Dat kost je een beetje, maar niet te veel.", "That costs you a little, but not too much."),
    ("Zozo. De bankier glimlacht een klein beetje.", "Well well. The banker smiles, just a little."))
add("react_bad",
    ("Auw! Dat doet pijn.", "Ouch! That hurts."),
    ("Oei, dat is een flink bedrag.", "Oops, that's a big one."),
    ("Nee hè! Daar ging een mooi bedrag.", "Oh no! There goes a nice amount."),
    ("Het publiek zucht. Jammer!", "The audience sighs. What a shame!"),
    ("Dat had ik liever niet gezien.", "I'd rather not have seen that."))
add("react_awful",
    ("Oh nee! Daar gaat een enorm bedrag!", "Oh no! There goes a huge amount!"),
    ("Dat is een klap. Maar het spel is nog niet voorbij!", "That's a blow. But the game isn't over yet!"),
    ("Pijnlijk, pijnlijk, pijnlijk!", "Painful, painful, painful!"),
    ("Ik hoor de bankier lachen aan de telefoon...", "I can hear the banker laughing on the phone..."),
    ("Diep ademhalen. We gaan gewoon door.", "Deep breath. We just keep going."))

add("million_alive",
    ("De miljoen is nog steeds in het spel!", "The million is still in play!"),
    ("En vergeet niet: de miljoen staat er nog!", "And don't forget: the million is still out there!"),
    ("Eén miljoen euro, nog steeds te winnen!", "One million euros, still up for grabs!"))
add("streak",
    ("Je bent goed bezig! Al drie lage bedragen op rij!", "You're on a roll! Three low amounts in a row!"),
    ("Wat een reeks! Heb je soms röntgenogen?", "What a streak! Do you have X-ray vision or something?"),
    ("Lekker bezig! Blijf zo doorgaan!", "Great going! Keep it up!"))
add("idle",
    ("Neem gerust de tijd.", "Take your time."),
    ("Geen haast. Het publiek wacht graag.", "No rush. The audience is happy to wait."),
    ("Twijfel je? Dat snap ik helemaal.", "Having doubts? I completely understand."),
    ("Wijs naar een koffer en druk op de trekker.", "Point at a case and pull the trigger."))

# ---------------------------------------------------------------- the banker
add("banker_call",
    ("De telefoon gaat! De bankier wil iets zeggen.", "The phone is ringing! The banker has something to say."),
    ("Daar is hij weer... de bankier!", "Here he is again... the banker!"),
    ("Stilte in de zaal, alsjeblieft. De bankier belt.", "Quiet please, everyone. The banker is calling."),
    ("Ring ring! Ik denk dat ik al weet wie dat is.", "Ring ring! I think I already know who that is."),
    ("Even opnemen. Hallo? Ja... ja... aha... interessant.", "Let me pick up. Hello? Yes... yes... aha... interesting."),
    ("De bankier heeft goed opgelet, en hij heeft een bod voor je.", "The banker has been watching closely, and he has an offer for you."))
add("offer_pre",
    ("De bankier biedt je...", "The banker offers you..."),
    ("Het bod is...", "The offer is..."),
    ("Hij zegt... en ik citeer...", "He says... and I quote..."),
    ("Op tafel ligt...", "On the table is..."),
    ("De bankier wil je geven...", "The banker wants to give you..."))
add("offer_low",
    ("Dat is een gierig bod, als je het mij vraagt.", "That's a stingy offer, if you ask me."),
    ("Hmm, de bankier probeert je een beetje af te schepen.", "Hmm, the banker is trying to fob you off."),
    ("Niet bepaald gul, die bankier.", "Not exactly generous, that banker."))
add("offer_fair",
    ("Dat is eerlijk gezegd een behoorlijk bod.", "Honestly, that's a decent offer."),
    ("Een redelijk bod. Moeilijk, hè?", "A reasonable offer. Tough, isn't it?"),
    ("Daar kun je toch leuke dingen van doen.", "You could do some nice things with that."))
add("offer_high",
    ("Wauw! De bankier is gul vanavond!", "Wow! The banker is feeling generous tonight!"),
    ("Dat is een heel mooi bod. Echt waar.", "That's a really nice offer. Truly."),
    ("Zo! Daar moet je wel even over nadenken.", "Whoa! You'll want to think about that one."))
add("offer_q",
    ("Neem je het bod aan, of speel je door?", "Do you take the offer, or play on?"),
    ("Wat wordt het? Geld pakken, of doorgaan?", "What will it be? Grab the money, or keep going?"),
    ("Jij bent aan zet. Aannemen, of doorspelen?", "Your move. Take it, or play on?"),
    ("De vraag van de avond: pak je het geld?", "The question of the night: do you take the money?"),
    ("Het publiek houdt de adem in. Wat doe je?", "The audience holds its breath. What will you do?"))
add("take",
    ("Je neemt het bod aan! Gefeliciteerd!", "You take the offer! Congratulations!"),
    ("Het geld is van jou! Applaus!", "The money is yours! Applause!"),
    ("Deal gesloten! Je gaat met een mooi bedrag naar huis.", "It's a deal! You're going home with a nice sum."),
    ("Verstandig! Geld in de hand is nooit in de koffer.", "Sensible! Money in your hand beats money in a case."),
    ("Handtekening gezet! De bankier hangt op.", "Signed and sealed! The banker hangs up."))
add("playon",
    ("Je speelt door! Moedig!", "You play on! Brave!"),
    ("Nee bedankt, meneer de bankier! We gaan verder!", "No thank you, mister banker! We carry on!"),
    ("Het avontuur gaat door!", "The adventure continues!"),
    ("Dapper! Ik hoop dat het goed afloopt.", "Bold! I hope it works out."),
    ("Weg met dat bod! Op naar de volgende ronde!", "Away with that offer! On to the next round!"),
    ("Jij durft! Het publiek is helemaal achter je.", "You've got nerve! The audience is right behind you."))

# ---------------------------------------------------------------- endgame
add("final_intro",
    ("Nog maar twee koffers over: die van jou, en de laatste op het podium.", "Only two cases left: yours, and the last one on the stand."),
    ("Dit is het. Twee koffers. Twee bedragen. Eén beslissing.", "This is it. Two cases. Two amounts. One decision."),
    ("We zijn bij het laatste moment. Het wordt nu echt spannend.", "We've reached the final moment. Now it gets really tense."))
add("swap_q",
    ("Wil je je eigen koffer houden, of ruilen met de laatste koffer?", "Do you want to keep your own case, or swap it for the last one?"),
    ("Houden of ruilen? Denk goed na!", "Keep or swap? Think carefully!"),
    ("Laatste kans: blijf je trouw aan je koffer, of ruil je?", "Last chance: do you stay loyal to your case, or do you swap?"))
add("keep",
    ("Je blijft trouw aan je eigen koffer!", "You stay loyal to your own case!"),
    ("Houden! Je hebt er vertrouwen in.", "Keep it! You believe in it."),
    ("Je eigen koffer, tot het einde. Mooi!", "Your own case, right to the end. Lovely!"))
add("swap",
    ("Je ruilt! Wat een plotwending!", "You swap! What a plot twist!"),
    ("Ruilen! Durf jij, zeg.", "Swap! You've got guts."),
    ("Een nieuwe koffer voor jou! Spannend!", "A new case for you! Exciting!"))
add("reveal_own",
    ("En nu het moment van de waarheid. In jouw koffer zat...", "And now the moment of truth. Inside your case was..."),
    ("Laten we je koffer openen. Er zat in...", "Let's open your case. Inside was..."),
    ("Je eigen koffer, al die tijd naast je. Er zat in...", "Your own case, right beside you all along. Inside was..."),
    ("Spanning! Wat zat er in jouw koffer? Het was...", "The tension! What was in your case? It was..."))
add("verdict_good",
    ("Je hebt de bankier verslagen! Wat een goede deal!", "You beat the banker! What a great deal!"),
    ("Slim gespeeld! Je deal was meer waard dan je koffer.", "Smartly played! Your deal was worth more than your case."),
    ("Perfect getimed! De bankier baalt enorm.", "Perfect timing! The banker is gutted."))
add("verdict_bad",
    ("Oei, in je koffer zat meer. Maar geld is geld!", "Oops, your case held more. But money is money!"),
    ("Ach, achteraf is het makkelijk praten. Je gaat toch met geld naar huis!", "Ah, hindsight is easy. You're still going home with money!"),
    ("De bankier wint deze keer. Maar jij hebt een mooi bedrag!", "The banker wins this time. But you've got a nice sum!"))
add("win_big",
    ("Ongelooflijk! Je gaat naar huis met een fortuin!", "Incredible! You're going home with a fortune!"),
    ("Wat een avond! Het publiek staat op de stoelen!", "What a night! The audience is on their feet!"),
    ("Gefeliciteerd! Dit vergeet je nooit meer!", "Congratulations! You'll never forget this!"))
add("win_mid",
    ("Een heel mooi bedrag! Goed gespeeld!", "A very nice amount! Well played!"),
    ("Daar kun je een mooie vakantie van boeken!", "You can book a lovely holiday with that!"),
    ("Netjes! Je kunt trots zijn.", "Nicely done! You can be proud."))
add("win_small",
    ("Nou ja, het gaat om de gezelligheid, toch?", "Oh well, it's the fun that counts, right?"),
    ("Niet het bedrag waar we op hoopten, maar wat een spanning!", "Not the amount we hoped for, but what a thrill!"),
    ("Volgende keer beter! De koffers wachten op je.", "Better luck next time! The cases will be waiting."))
add("goodbye",
    ("Bedankt voor het spelen! Tot de volgende keer bij Koffer Koorts!", "Thanks for playing! See you next time on Case Fever!"),
    ("Dat was het weer! Dag allemaal!", "That's all for now! Goodbye, everyone!"),
    ("Tot ziens, en blijf van die koffers dromen!", "Goodbye, and keep dreaming of those cases!"))

# ---------------------------------------------------------------- party mode
add("mp_welcome",
    ("Welkom bij de feesteditie! Iedereen heeft een eigen controller, en een eigen koffer.",
     "Welcome to the party edition! Everyone gets their own controller, and their own case."),
    ("Het is feest! Meerdere spelers, één bankier. Wie gaat er met het meeste geld naar huis?",
     "It's party time! Several players, one banker. Who goes home with the most money?"))
add("mp_rules",
    ("Om de beurt open je een koffer. Na elke ronde krijgt iedereen een bod, en kiest iedereen tegelijk, in het geheim. Wie het meeste geld heeft aan het eind, wint!",
     "You take turns opening cases. After every round everyone gets an offer, and everyone decides at the same time, in secret. Whoever has the most money at the end wins!"),)
for p in range(1, 5):
    nlp, enp = nl_num(p), en_num(p)
    add(f"mp_pick_{p}",
        (f"Speler {nlp}, kies je eigen koffer.", f"Player {enp}, pick your own case."),
        (f"Speler {nlp}, welke koffer wordt de jouwe?", f"Player {enp}, which case will be yours?"))
    add(f"mp_turn_{p}",
        (f"Speler {nlp}, jij bent aan de beurt.", f"Player {enp}, it's your turn."),
        (f"Speler {nlp}, kies een koffer om te openen.", f"Player {enp}, choose a case to open."),
        (f"En nu speler {nlp}!", f"And now, player {enp}!"))
    add(f"mp_offer_{p}",
        (f"Speler {nlp}, jouw bod is...", f"Player {enp}, your offer is..."),)
    add(f"mp_took_{p}",
        (f"Speler {nlp} pakt het geld!", f"Player {enp} takes the money!"),
        (f"Speler {nlp} sluit een deal!", f"Player {enp} makes a deal!"))
    add(f"mp_case_{p}",
        (f"Speler {nlp}, in jouw koffer zat...", f"Player {enp}, inside your case was..."),)
    add(f"mp_winner_{p}",
        (f"Speler {nlp} wint het spel! Gefeliciteerd!", f"Player {enp} wins the game! Congratulations!"),
        (f"De winnaar van vanavond is... speler {nlp}!", f"Tonight's winner is... player {enp}!"))
add("mp_offer_all",
    ("De bankier doet iedereen een bod! Kies allemaal tegelijk, in het geheim.", "The banker has an offer for everyone! Everybody choose at the same time, in secret."),
    ("Biedingen voor iedereen! Wie pakt het geld, en wie speelt door?", "Offers for everyone! Who takes the money, and who plays on?"))
add("mp_choose_now",
    ("Kies nu allemaal: aannemen of doorspelen.", "Everyone choose now: take it, or play on."),
    ("Iedereen, maak je keuze!", "Everyone, make your choice!"))
add("mp_reveal_choices",
    ("Laten we kijken wie er een deal sluit!", "Let's see who makes a deal!"),
    ("De keuzes zijn binnen. En...", "The choices are in. And..."))
add("mp_all_playon",
    ("Niemand neemt het bod. Jullie zijn dapper!", "Nobody takes the offer. You're all so brave!"),
    ("Iedereen speelt door! De bankier is niet blij.", "Everyone plays on! The banker is not happy."))
add("mp_end",
    ("Alle koffers zijn open. Tijd voor de uitslag!", "All the cases are open. Time for the results!"),
    ("Dat was de laatste ronde. Wie heeft er gewonnen?", "That was the last round. Who has won?"))
add("mp_tie",
    ("Het is gelijkspel! Wat een avond!", "It's a tie! What a night!"),)

# ---------------------------------------------------------------- amounts
for c in AMOUNTS_CENTS:
    add(f"amt_{c}", amount_words(c))
for e in offer_values():
    if f"amt_{e*100}" not in G:
        add(f"amt_{e*100}", amount_words(e * 100))


def subtitle_for(group, variant_text_pair):
    if group.startswith("amt_"):
        c = int(group[4:])
        return amount_subtitle(c)
    return variant_text_pair
