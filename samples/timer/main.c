#include <util/stream.h>
#include <util/alloc.h>

#include <packos/sys/contextP.h>
#include <packos/packet.h>
#include <timer.h>
#include <iface-native.h>

#include <schedulers/basic.h>

static const char* quotes[]=
  {
    "Beware of dragons, for you are crunchy and good with ketchup.",
    "There are many intelligent species in the cosmos.  All are owned by cats.",
    "I am Homer of Borg.  Prepare to be assi... Mmm, doughnuts!",
    "\"She gets kidnapped. He gets killed. But it turns out okay.\" -- _Princess Bride_ poster",
    "\"Chris is the most self-effacing guy I know.\" \"Well, I'm not *that* good at it.\"",
    "Never underestimate the power of human stupidity.  --I forget who",
    "Until you stalk and overrun, you can't devour anyone. --Hobbes",
    "Earth: love it or leave it.",
    "What do you mean, *you're* a solipsist?",
    "No matter how subtle the wizard, a knife in the shoulderblades will cramp his style.",
    "Think of it as evolution in action.",
    "Help stamp out vi in our lifetime!",
    "This is the .sig that says... Ni!",
    "Newt Nuggets!",
    "You buttered your bread, now lie in it.",
    "I'm a .sig virus...and, boy, am I tired!",
    "But this one goes to 11x.",
    "Cogito ergo Spud.  (I think, therefore I yam.)",
    "I will not buy this .signature, it is scratched.",
    "Never do card tricks for your poker buddies.",
    "Brought to you by the letter Q.",
    "Dave Barry for President! He'll Keep Dan Quayle.  (OK, it's old)",
    "We want forty million helicopters and a dollar! --\"Dinosaurs\"",
    "E pui muove! -- Galileo",
    "\"I think we'd get fewer bug reports if we stopped selling our software off planet.\"",
    "Belief is not relevant to truth.",
    "If God had not given us duct tape, it would have been necessary to invent it.",
    "The Player's Litany: The Wind is the Storm, and the Storm is Data, and the Data is Life. --Daniel Keys Moran",
    "In the long run, there is no middle ground between justice and genocide.",
    "This space intentionally left blank.",
    "This space intentionally not left blank.",
    "\"Hastur was paranoid, which was simply a sensible...well-adjusted reaction to living in Hell.\" --_Good Omens_",
    "Unless. -- The Lorax",
    "\"Simply vanished--like an old oak table.\" --Lord Percy, _Black Adder II_",
    "\"God does not play games with His loyal servants.\"  \"Whoo-ee, where have you *been*?\" --_Good Omens_",
    "Please do not look into laser with remaining eyeball.",
    "Reality is what refuses to go away when I stop believing in it.",
    "Yes, sir, we've graphed the data.  It's a smiley face, sir.",
    "If you're going to walk on thin ice, you might as well *dance*!",
    "A man's concepts should exceed his vocabulary, or what's a metaphor?",
    "Misanthropology: the study of why people are so stupid.",
    "Please do not adjust your mind--reality is malfunctioning.",
    "I'm not imaginary.  I'm ontologically challenged.",
    "\"How many lifeboats are there?\" \"None.\" \"Did you *count* them?\" \"Twice.\"",
    "I'm not a bibliophile, I'm a bibliophiliac.  Put me in a bookstore, & my wallet bleeds.",
    "Id est fabula nostra, et non mutabimus eam! --House Falconguard and Affiliated Scum",
    "The good are innocent and create justice.  The bad are guilty, and invent mercy.",
    "\"I said no camels! That's 5 camels! Can't you count?\" -- Indiana Jones & TLC",
    "\"You gotta learn to dance before you learn to crawl!\" -- Meatloaf, \"Everything Louder than Everything Else\"",
    "A ship is safe in a harbor, but that's not what a ship is for.",
    "Any sufficiently rigged demo is indistinguishable from advanced technology.",
    "The Net regards censorship as damage, and routes around it. -- John Gilmore",
    "Guide us, oh holy Lemming Herder!",
    "Vote for Ron, and nobody gets hurt! --actual campaign poster from Chicago",
    "You! Out of the gene pool!",
    "\"Fate just isn't what it used to be.\" --Hobbes",
    "The cheapest, fastest, most reliable components of a computer system are those that aren't there.--Gordon Bell",
    "\"There will be no more there.  We will all be here.\"--networkMCI ad",
    "Almost no one has ever wanted a 1/4\" drill bit; all they ever wanted was a 1/4\" hole.",
    "Nondeterminism may or may not mean never having to say you're wrong.",
    "Never mind the GUIs--Unix won't be for the masses until we fix backspace & delete.",
    "Vlad was not a vampire, but that's the only nice thing that could be said about him.",
    "\"Genius, Brain! But what if the dragon eats us?\" \"That would alter our plans.\"",
    "\"What now, Brain?\" \"We should flee in terror.  Yes, that would be the wisest course.\"",
    "I imagine the wages of sin *are* death, but by the time they take taxes out it's just sort of a tired feeling. --Paula Poundstone",
    "This is not a self-referential .signature.",
    "\"The Reality Check's in the mail.\" --L. Peter Deutsch",
    "\"I only wish I had time to get married myself, as I've told m'wife many's the time.\"",
    "If all the world's a stage, I want to operate the trap door.",
    "\"Your reality, sir, is lies & balderdash, and I am pleased to say I have no grasp on it whatsoever!\" --Baron Munchausen",
    "Know your limits, then destroy 'em.",
    "Just because we are out to get you does not mean you are not paranoid.",
    "He wondered if Elli was going to buy that explanation. His taste for heavily-armed girlfriends did have its drawbacks.",
    "Excuse me. I've been dead lately & my brain isn't working too well. --Miles Vorkosigan",
    "Organ transplants are best left to the professionals.",
    "Sleep is for wimps--healthy, well-adjusted wimps, but wimps nonetheless.",
    "Peace, justice, morality, culture, family, sport, and the extinction of all other life forms!",
    "Round up the usual disclaimers.",
    "Q: What goes \"Pieces of 7! Pieces of 7!\"? A: A parroty error.",
    "Maybe only the solipsists are imaginary.",
    "All I ask is a chance to prove that money can't make me happy.",
    "\"You're nothing but a pack of ringleaders!\" --_Wyrd Sisters_, Terry Pratchett",
    "Stupid, adj.: Losing $25 on the game and $50 on the instant replay.",
    "Diplomacy:  The art of letting someone else have your way",
    "Don't anthropomorphize computers. We don't like it.",
    "They prayed for their fates to be quick, painless, and, ideally, someone else's.",
    "I have strong opinions about ambivalence.",
    "Among animals, it's eat or be eaten.  Among people, it's define or be defined.",
    "In the country of the blind, the one-eyed man is in therapy.",
    "Campbell's has it wrong--it's \"Never underestimate the power of *chocolate*\".",
    "\"Who died and made you king?\" \"My father.\"",
    "\"No, no, that's *not* a boat, that's Queen Victoria.\"",
    "World domination should never be left to chance.",
    "The problem with any unwritten law is that you don't know where to go to erase it.",
    "How many roads must a man walk down before he admits he is LOST?",
    "\"Time is money, and price is information.\" --Russ Nelson",
    "Solipsists have all the fun.",
    "The sooner you fall behind, the more time you'll have to catch up!",
    "Speak softly and carry an Illudium Q-32 Explosive Space Disintegrator.",
    "Due to circumstances beyond your control, you are master of your fate, and captain of your soul.",
    "\"We can't duplicate the bug.\" \"Have you tried the Xerox machine?\"",
    "If you're omniscient, how do you know?",
    "If jumping off a bridge was \"the industry standard\", would you do it?",
    "\"Collect call from reality, will you accept the--\" *click*",
    "There are footprints on the moon.  No feet, just footprints.",
    "A successful tool is one that was used to do something undreamed of by its author.",
    "If God had meant us to be in the Army, we would've been born with green, baggy skin.",
    "A mime is a wonderful thing to waste.",
    "Do not suspect that I am not human.",
    "I used to belong to a solipsism club, but I got bored & voted everybody else out.",
    "\"How quietly do you think we can nail these back in?\" --Calvin",
    "\"Where's your sense of adventure?\" \"Hiding under the bed.\"",
    "\"Where's your sense of adventure?\" \"In front of a roaring fire with a cup of cocoa.\"",
    "We must be devious, cunning, inventive... too bad we're us.",
    "\"How can one conceive of a one party system in a country that has over 200 varieties of cheese?\" --Charles de Gaulle",
    "\"Lollygaggers will be shot on sight!\" \"I didn't say that!\" \"I was anticipating your whim.\"",
    "I'm on a Mission From God to Save The World from megalomania!",
    "That is correct. I'm out of fuel. My landing gear is jammed.  And there's an unhappy bald eagle loose in the cockpit.",
    "\"But she calls her ship _Mercy of the Goddess_!\" \"Kali.\" \"Oh.\"",
    "Both candidates are better than a megalomaniac mutant lab mouse bent on world domination...but it's pretty close.",
    "\"The Elliot 903 was the first portable I ever used, though it needed a furniture van to *make* it portable.\" --Will Rose",
    "\"And bring the search warrant.\" \"You mean the sledgehammer, sir?\" \"Yes.\"",
    "\"If nobody believes what I say, I feel ineffective.\" \"Oh, I don't believe that.\"",
    "\"Dinner Special: Turkey $2.35; Chicken or Beef $2.25; Children $2.00.\"",
    "Stock up and save! Limit one.",
    "Illiterate? Write today for free help!",
    "Two words that do not go together: \"Memorial Cookbook\".",
    "\"What we have here is a failure to assimilate.\" --Cool Hand Locutius",
    "Seen in computer peripheral ad: \"User-friendly dip switches!\"",
    "I am destined to build a bridge to China out of live sheep.",
    "\"The avalanche has already started. It is too late for the pebbles to vote.\" --Kosh",
    "No matter how tempting the prospect of unlimited power, I will not consume any energy field bigger than my head.",
    "Veni, Vidi, Ridi: I came, I saw, I mocked.",
    "\"Baldric, how did you manage to find a turnip that cost 400,000 pounds?\" \"Well, I had to haggle.\" --Blackadder III",
    "\"This [OLE] isn't an API, this is a shaggy dog story in C!\" -- Unknown",
    "The plural of mongoose is polygoose.",
    "Whose cruel idea was it for the word \"lisp\" to have an \"S\" in it?",
    "If you choke a Smurf, what color does it turn?",
    "If at first you don't succeed, don't go skydiving.",
    "Life is like a metaphor.",
    "So what's the gene for belief in genetic determinism?",
    "\"It takes a special kind of guy to rig an election and then lose.\" --Cecil Adams",
    "When rats leave a sinking ship, where exactly do they think they're going?",
    "It's not an optical illusion, it just looks like one.",
    "You're just jealous because the voices only talk to me.",
    "Anyone who has a conditioned response immediately thinks of Pavlov.",
    "See what bilateral symmetry can do for YOU!",
    "This sentance has threee errors.",
    "Some days, it just doesn't pay to gnaw through the straps.",
    "\"The Empire has a tyrannical and repressive government.\" \"What form of government is that?\" \"A tautology.\"",
    "What if there were no hypothetical questions?",
    "\"I lost an 7-foot boa constrictor once in our house.\" --Gary Larson on his youth",
    "NT's lack of reliability is only surpassed by its lack of scalability. --John Kirch",
    "A computer without Windows is like a chocolate cake without mustard.",
    "Rope is rope, and string is string, and never the twine shall meet.",
    "\"Music is not a noun, it's a verb.\" --John Perry Barlow",
    "All your problems can be solved by not caring!",
    "The voices say I'm not schizophrenic, but they're not sure about you.",
    "But how do we know destroying the Van Allen belt will kill all life on Earth if we don't try it?",
    "Amazing Fact: If there are three cats in a room, they will form a triangle!",
    "\"This horse has made a career out of being dead.\" -- Harald Alvestrand",
    "\"Sometimes we in the media have to give the copycat criminals a little kick-start.\" -- Tina the Tech Writer, in _Dilbert_",
    "\"Why did you become a spokesmodel?\" \"Oh, well, I've always liked pointing.\" -- _LA Story_",
    "\"HTTP is what happens in the absence of good design.\" -- Keith  Moore",
    "\"Millions of dollars of profits is what happens in the absence  of good design.\" -- Keith Moore",
    "\"If it failed then, there's no reason to think we can't make it fail now.\" -- Colin Benson",
    "Two words that inspire the same feeling of dread are synominous.",
    "Using strong crypto on the Internet is like using an armored car to transport money from someone living in a tent to someone living in a cardboard box.",
    "\"All gateways lose information. Some do it more efficiently than others.\" -- Einar Stefferud",
    "For an engineer, paranoia is not an affliction; it's a tool.",
    "Go not to the Vorlons for advice, for they will say both no and sherbert.",
    "Te audire no possum. Musa sapientum fixa est in aure.",
    "\"If there's anything The Flintstones have taught us, it's that pelicans can be used to mix cement.\" -- Homer Simpson",
    "\"I use emacs, which might be thought of as a thermonuclear word processor.\" -- Neal Stephenson",
    "McDonald's, which does not wait on your table, does not cook your food  to order, and does not clear your table, came up with the slogan \"We Do It All For You.\"  -- Dave Barry",
    "\"Never interrupt your enemy when he is making a mistake.\"  -- Napoleon Bonaparte",
    "Q: How do you play religious roulette? A: You stand around in a circle and blaspheme, and see who gets struck by lightning first.",
    "You know that logic puzzle with the fox, the goose and the grain? What kind of farmer owns a *fox*?",
    "For the life of me, I've never understood why people dismiss the importance of \"semantics\" in communication.  After all, if semantics had no purpleness, you'd slitheringly go bold at by-sit it I'm climbing. -- Tangwystyl",
    "\"The struggle is always worthwhile, if the end be worthwhile and the means honorable; foreknowledge of defeat is not sufficient reason to withdraw from the contest.\" -- Adron e'Kieron, by Steven Brust",
    "\"A dog will eat pretty much anything; one major reason why there are no restaurants for dogs is that the customers would eat the menus.\" -- Dave Barry",
    "I'm a zygote, he's a zygote, she's a zygote, wouldn't you like to be a zygote, too?",
    "\"Stop slouching towards Bethlehem!\" -- The Mother of the Beast",
    "*BOOM* \"Thank you, Beaker.  Now we know that is definitely too much gunpowder.\"  -- Dr. Bunsen Honeydew",
    "\"Fly Heisenberg Airlines! We may not know where we are, but we're making real good time.\" -- _Synners_ by Pat Cadigan",
    "\"Call me a Nervous Nellie, but I am concerned about the sale of nuclear arms in my general neighborhood.\" -- Dave Barry",
    "Linux: the Unix defragmentation tool.",
    "\"Power corrupts; Powerpoint corrupts absolutely.\" -- Vint Cerf",
    "News flash: Linux now implements RFC-1149, IP over Carrier Pigeon!",
    "The light at the end of the tunnel is a terawatt laser cannon.",
    "Elves are cool.  They make great cookies. Take six pounds of chocolate, three pounds of flour, and 3 pounds of dried finely chopped elf...",
    "Agents of Chaos, Inc.  Every office independently owned & operated.",
    "\"What a plan! Simple, yet insane!\" -- _Monsters, Inc._",
    "\"I'm sorry, Wizowsky, Randall says I'm not supposed to talk to victims of his evil plots.\" -- _Monsters, Inc._",
    "\"I'm off to wander the streets aimlessly.  I'll be taking my usual route.\" -- Lillith, _Cheers_",
    "\"My parents raised me not to accept being cheated on by a robot.\" -- R2-D2's girlfriend, in \"R2-D2's: Beneath the Dome\"",
    "\"[The Rolling Stones] is not a pretty-boy band. If they've had any plastic surgery, it was apparently done at Home Depot.\" -- Dave Barry",
    "\"English doesn't borrow from other languages. English follows other languages down dark alleys, knocks them over, and goes through their pockets for loose grammar.\" -- various attributions",
    "One must understand recursion to understand recursion.   To understand tail recursion, one must understand tail recursion.",
    "If we don't believe in freedom of expression for people we despise, we don't believe in it at all. -- Noam Chomsky",
    "\"So we've strapped a Patriot missile onto Snorky here.\" \"You said that was a PROP!\" \"It's a *functional* prop.\" -- What's New, by Phil Foglio.",
    "They called me MAD at the commitment hearing!",
    "Lieberman is a Republican in disguise. Clark is a Republican in Groucho glasses.",
    "\"I take it the army of super mutants didn't work out?\" \"Bah.  Those geneticists had the evil genius of pocket lint.\" -- GPF",
    "\"You *will* stop Alanis Morissette from developing a mining colony in my nostrils, won't you?\" \"It's next on my list.\" -- Newshounds",
    "\"The head of a caribou is not a valid form of ID.\" -- Sluggy Freelance",
    "\"What luck! A laser cannon!\" -- Sluggy Freelance",
    "\"Your candidate spelled 'tomato' with four 'Q's, seven 'Z's, and a drawing of a lemur!\" -- Krazy Larry",
    "\"Have you seen my copy of 'Playing God for Dummies'?\" -- Krazy Larry",
    "\"I've come to see the idea of a government teaching history a lot like Enron being trusted to audit itself.\" -- siderea",
    "Loki, Eris, and Coyote walk into a bar, and the bartender says \"Order, please.\"",
    "Nothing screams \"poor workmanship\" like wrinkles in the duct tape.",
    "\"[W]e are hard-pressed to say exactly what its specs are, since the write-up seems to have gone through several Babelfish translations.\" -- Engadget",
    "\"What are you playing?\" \"Ouija-board Scrabble!\" -- Sluggy Freelance",
    "\"But somewhere in there, it made the migration in my mind from 'entertainment' to 'specimen'.\" -- siderea"
  };

static const int numQuotes=sizeof(quotes)/sizeof(quotes[0]);

static void mooseProcess(void)
{
  int i;
  PackosError error;
  Timer timer;
  UdpSocket socket;

  if (UtilArenaInit(&error)<0)
    return;

  {
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "mooseProcess(): IpIfaceNativeNew: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "mooseProcess(): IpIfaceRegister: %s\n",
                PackosErrorToString(error));
        return;
      }
  }

  socket=UdpSocketNew(&error);
  if (!socket)
    {
      UtilPrintfStream(errStream,&error,"mooseProcess(): UdpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(socket,PackosAddrGetZero(),5000,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"mooseProcess(): UdpSocketBind(): %s\n",
              PackosErrorToString(error));
      UdpSocketClose(socket,&error);
      return;
    }

  timer=TimerNew(socket,1,0,5,true,&error);
  if (!timer)
    {
      UtilPrintfStream(errStream,&error,"mooseProcess(): TimerNew(): %s\n",
              PackosErrorToString(error));
      UdpSocketClose(socket,&error);
      return;
    }

  i=0;
  while (true)
    {
      PackosPacket* packet=UdpSocketReceive(socket,0,true,&error);
      if (!packet) continue;

      UtilPrintfStream(errStream,&error,"The Moose Says: %s\n",quotes[i]);
      i++;
      i%=numQuotes;
    }
}

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext moose=PackosContextNew(mooseProcess,"moose",error);
  if (!moose)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(moose): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,moose,error);

  return 0;
}

int testMain(int argc, const char* argv[])
{
  PackosError error;

  PackosKernelLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),&error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
