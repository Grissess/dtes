# dtes

_"Discrete Time Event Simulator"_

This is a _very_ simple algebra for running a simulation wherein events are
randomly assigned to actors in rounds. In particular, it's a competitor to
Brant Steele's old ["hunger games" simulator][sim].

The simulator is designed to be internally stateless. At every step ("round"):

1. The world is read in. The world contains _everything_, including the
   definition of all possible events.
2. Some events are chosen and "bound" (fitting actors are inserted into the
   placeholders).
3. The bindings are consulted for the changes they cause, and thus change the
   world.
4. The world is emitted.
5. The events' messages are emitted.

This process is designed such that the world emitted at step 4 can be read in
step 1, allowing the simulator to iterate indefinitely. The base simulator has
no concept of an end or "winning" condition, and will gladly keep going for as
long as you are willing to run it.

# World Format

The world is defined in a loosely-defined plain text format that allows
operator intervention if necessary (for example, for bootstrapping, or for
configuring "special" events).

What follows should cover most scenarios.

```
pronouns {
  female: she her her herself sing
  male: he him his himself sing
  neuter: it it its itself sing
  nonbin: they them their themself plur
  plural: they them their themselves plur
}
players {
  foo: Foo(female)[hunter]
  bar: Bar(male)[dead, weapon:gun]
  baz: The Big Baz(nonbin)[]
}
relations {
  likes: undir {
    foo bar
    bar foo
  }
  revenge: dir {
    foo baz
  }
  same: undir reflex {
    foo foo
    bar bar
    baz baz
  }
}
world [day, hot]
events {
  nonlethal: {
    needs {
      attacker: [!dead]
      victim: [!dead]
    }
    message {$attacker bludgeon[sing=s] $<victim>.}
  }
  lethal: {
    needs {
      attacker: [!dead]
      victim: [!dead]+[dead]
    }
    message {$attacker kill[sing=s] $<victim>.}
  }
  uses_property: {
    needs {
      attacker: [!dead, weapon:]
      victim: [!dead]+[dead]
    }
    message {$attacker kill[sing=s] $<victim> with <(attacker)p> $<.weapon>.}
  }
  changes_property: {
    needs {
      a: [!dead, weapon:]-[weapon:]
      b: [!dead, !weapon:]+[weapon:$<a.weapon>]
    }
    rel { a:likes:b }
    message {$a find[sing=s] $b unarmed, and give[(a)sing=s] <(b)o> <(a)p> $<.weapon>.}
  }
  world_dependent: {
    needs { a: [!dead] }
    world [hot]
    message {$a find[sing=s] nobody, but is afflicted by the heat. <S> retreat[sing=s] into a shady cave.}
  }
  world_change: {
    world [day]+[night]-[day]
    message {The sun sets.}
  }
  random_chance: {
    world []+[party]
    chance 1/5
    message {The party starts!}
  }
  relation_based: {
    needs { a: [!dead] b: [!dead] }
    rel { !a:likes:b +a:likes:b }
    message {$a and $b have grown fond of each other.}
  }
}
```

Sections (such as `pronouns`, `players`, `world`, `relations`, and `events` can
occur in any order. However, as they are read top-to-bottom, any definition
that _depends_ on a previous definition requires the previous definition to
have been done. (This is the case with the `pronouns` referenced in `players`.)
Attributes, which are given to players and matched in events, are string
values, and thus not "references" in that sense. The same applies to "world
attributes". See the following sections for more details.

> [!NOTE]
> Technically, you can even include the same section multiple times. In theory,
> this can be used to _add_ more entries, but _must not be used to replace
> them_, as this will break references. The output format will always have
> single sections in this order, and you should not rely on this feature.

## Pronouns

A single `Pronouns` entry defines a family of pronouns and conjugations to use
with a player. They are listed in this order; examples are given for English
plural third person:

- **Subject**: **They** were ecstatic.
- **Object**: I saw **them**.
- **Possessive**: This is **their** room.
- **Reflexive**: They saw **themselves**.
- **Tense**: `plur` (Arbitrary, explained below.)

So a single line defining these pronouns might look like:

```
plural: they them their themselves plur
```

which defines this set of pronouns using the `plural` identifier, referenced by
players below.

The first four entries correspond to parts of speech; the fifth is a "tense",
used to adjust verb conjugations. In English, at least, verbs conjugate based
on the numerosity of the subject ("singular" or "plural"). So, for example:

- He **walks** to the park. _(singular)_
- They **walk** to the park. _(plural)_

The tense field has no intrinsic meaning, but can assist with conjugations.
It's theoretically possible to have one tense per pronoun, which may be useful
for supporting other languages.

> [!NOTE]
> It's a noted archaic inconsistency that they/them pronouns, often used for
> nonbinary people even in singular form, still conjugate plural, as encoded
> above.

The example pronouns are compatible with many use cases, and can be used as-is.

## Players

Each `Player` entry defines an "actor", a participant in the simulation against
which events are ascribed. They have a **name**, a set of **pronouns**, a set
of **attributes**, and a map of **properties**. Attributes are arbitrary
strings associated with the player, which can represent traits (`strong`),
professions (`hunter`), states (`dead`), and more. They exist to let event
authors filter on "allowed" actors, and have no intrinsic meaning.

The format is:

```
identifier: name(pronouns)[attribute, attribute, attribute]
```

`name` may contain spaces, but can't start with a space. `pronouns` should
refer to the identifier of a `Pronoun` entry defined earlier in the file. The
order of `attribute` declaration is unimportant, and redundant entries are
ignored. The `identifier` is used in `relations`.

If an attribute contains a `:`, it becomes a **property**, of the form
`key:value`. Properties have a name but are associated with a value; neither
can be empty.

## Relations

Relations entries each define a kind of relation, which is a property that
connects two players, **left** and **right**. Their format is:

```
identifier: dir/undir { a b c d ... }
identifier: dir/undir reflex { a a b c d d ... }
```

Relations can be **directed** or **undirected**; a relationship that is
undirected always has (right, left) given (left, right) as an edge. One might
say it is "commutative".

A relation can also be **reflexive**; it isn't by default, but this can be
added with `reflex`. Reflexive relations can contain pairs (a, a)--that is, a
player can be in the relation with themselves.

After the specification is a brace block with whitespace-separated `Player`
identifiers. Each pair is an edge ("in" the relation). For undirected
relations, `a b` implies `b a`, but this is not required--it is added
automatically (and will be part of the output).

Relations are a powerful tool, and can be used for complicated modeling, but
beware of the complexity this adds. Some examples:

- A relation `allies: undir { }` could encode alliances of convenience between
  players. These are generally understood to be mutual, so an undirected
  relation is a good fit.
- A relation `vengeance: dir { }` could define an especial enmity from one
  player to another. The "target" may not even be aware of this enmity, so a
  directed relation is convenient.
- A relation `same: undir reflex { }` could be used to contain an edge `a a`
  for each character. (Directionality doesn't matter for reflex edges.) This
  isn't very useful on its own, but can be used to filter events so that two
  `needs` references don't contain the same character. (When relations aren't
  needed, attributes are much more efficient.)

## World

The world also has a set of attributes, like a player. The `world` "section" is
not followed by a namespace, but a simple attribute list, much like a `Player`.
For example:

```
world [day, hot]
```

These attributes are good for global state (`hot`), "special events" (`feast`),
and the passage of time (`day`), among others. Like players, events can match
and change these attributes.

## Events

Events are the most complicated descriptions, in no small part because they
contain sub-blocks. An `Event` entry is generally

```
identifier: {
  needs { ... }
  rel { ... }
  world [...]
  chance a/b
  message {...}
}
```

The `identifier` is only used in debugging. `needs` defines the actors which
are needed to "complete" the event, and may be empty. `world` defines the
necessary state of the world. When the `Event` is successfully chosen and
bound, it will emit `message`, which may contain various placeholders. These
are described in more detail below.

### Needs

This section both defines necessary actors, including filtering based on
attributes, as well as changes to those attributes that happen as a result of
the event occurring. Each `Needs` entry is as follows:

```
identifier: [required,!disallowed]+[added]-[removed]
```

Each list may contain several comma-separated entries in any order. The first
list (the "match" list) is used to define attributes which are required to be
present or absent (those which must be absent are prefixed with `!`). For
example, `[hunter]` only allows a `Player` with attribute `hunter` to bind to
this `identifier`, and `[!dead]` requires a `Player` which does _not_ have the
`dead` attribute. A `Player` may (or may not) have any number of other
unmentioned attributes.

Optionally, following the "match" list, `+` may be followed by an attribute
list. When this `Event` is emitted, these attributes are added to the `Player`
bound in this position. Also optionally, a `-` attribute list can be used to
_remove_ attributes from a matching `Player`. When both are used, the `+` list
must precede the `-` list.

Without loss of generality, the attributes in these lists can contain a colon;
in this case, they refer to properties. If the value is omitted from the match
list, the _presence_ is tested for (matching or non-matching with `!`).
Similarly, if the value is omitted from a deletion, the property is deleted
regardless of its value. (Normally it would be deleted only if the value
matched.) Adding a property will also be done only if the value is non-empty.

The value part of a property is expanded as with `message` below; in
particular, one can expand properties of other actors, so `b:
[!weapon:]+[weapon:$<a.weapon>]` will set `b`'s `weapon` property to `a`'s
`weapon` property. All other message expansions should work; it should be noted
that additions are processed before deletions, so `b` will still receive `a`'s
`weapon` even if `a` removes the property afterward (as with `a:
[weapon:]-[weapon:]`). As with `message`s, this is fully general, and any
properties can be expanded, cross-expanded, et cetera. Note that, if the
property expands to the empty string, the property is removed instead.

Changes to players' attributes are atomic; they are only done after the full
set of Events are chosen. This means that, within a single Round, one Event
cannot "trigger" another one to match when it did not before.

### Rel

This section contains relations. If this isn't empty, the semantics of matching
change; in particular, the matching algorithm is also much slower, so use this
only if needed.

Each whitespace separated word in this list is of the following forms:

- `left:rel:right`: for this event to fire, the `needs` refs (`left`, `right`)
  must be in the named relation `rel`.
- `!left:rel:right`: for this event to fire, the `needs` refs (`left`, `right`)
  must _not_ be in the named relation `rel`.
- `+left:rel:right`: when this event fires, players bound to `needs` refs
  (`left`, `right`) will be added to the named relation `rel`. In addition,
  this event will _not_ fire if `rel` is not reflexive and `left` and `right`
  bind to the same player.
- `-left:rel:right`: when this event fires, players bound to `needs` refs
  (`left`, `right`) will be removed from the named relation `rel`.

### World

The syntax is the same as a single entry in `needs`:

```
world [required,!disallowed]+[added]-[removed]
```

These match the world's "global" attributes, rather than any single player. The
semantics are otherwise the same.

### Chance

The `chance` entry defines two numbers:

```
chance multiplicity
chance multiplicity/unlikeliness
```

Both of these must be positive integers, and default to `1` if not specified.

- **Multiplicity**: The number of times this even can happen in one round.
  Since all copies participate in the shuffle, this also effectively makes it
  "more likely" relative to the other events.
- **Unlikeliness**: If this event is picked, it has to succeed on a
  1-in-unlikeliness random roll or be discarded. For example, unlikeliness 2 is
  a coinflip chance, and unlikeliness 4 is a 1-in-4 chance of being actually
  used if present in the shuffle order.

> [!WARNING]
> Multiplicity is presently implemented by duplicating a pointer to the event
> in the shuffle deck. Pointers are small (usually 8 bytes), but asking for
> thousands or millions of copies is asking for a crash, not to mention a very
> slow event shuffle.

Probabilistically, the ratio is _close_ to the expected value of this event
being chosen relative to a `1` baseline uniform of all other events. For
example, `chance 1/2` means this event only appears once in the event deck (and
thus can only happen at most once per round), but it may be ignored by a
coinflip. `chance 4/8`, however, means the event can happen up to 4 times, but
only if it passes a 1-in-8 check. The _expected_ value is still once every
other round, but the event can still happen far more times in an "unusually
lucky" round compared to the `chance 1/2` specification.

For events that have no `needs` specification (so-called "unassociated"
events), this is the only way to control their randomness. (They can also match
on `world` properties, but that matching is deterministic.)

### Message

The message is written to output for each Event after the new state of the
World is computed. Most text within this field is copied without
modification--and this includes newlines. A message containing `}` is not
allowed; `}` always marks the end of the message.

The following special forms exist:

- `$<identifier>`, `$identifier` _(only when followed by whitespace)_: Given a
  Player bound to the named slot in `needs`, this interpolates to that Player's
  name.
- `$<identifier.prop>`, `$identifier.prop` _(only when followed by
  whitespace)_: Expands to the `prop` property of the player bound to the named
  slot in `needs`. If that property doesn't exist or isn't defined, this
  expands to nothing.
- `[(identifier)t1=a/t2=b/t3=c]`: Given a Player bound to the named slot in
  `needs`, this inspects the Player's `Pronouns` "tense" field. If the tense
  matches one of the tenses (here `t1`, `t2`, or `t3`), the corresponding
  output to the right side of the `=` is written (here `a`, `b`, or `c`). If no
  tense matches, nothing is written. `(identifier)` may be omitted (see below).
- `<(identifer)x>`: Given a Player bound to the named slot in `needs`, this
  writes out a pronoun. `x` may be one of `s` (subject), `o` (object), `p`
  (possessive), or `r` (reflexive). Capitalizing the letter (`S`, `O`, `P`, or
  `R`) capitalizes the first letter of the pronoun. `x` may also be `'s`, in
  which case it emits the possessive particle `'s` unless the referenced
  actor's name ends with an `s` or `S`, in which case it emits a `'` instead.

`$<identifier>`, `$identifier` (including property references), and `<...>`
forms all set a context-sensitive "last player referenced"; subsequent
references depending on a player identifier (`[...]` and `<...>` forms) use
this implicitly if their parenthesized identifier is omitted. Similarly, the
property references may have their first identifier omitted to use this "last
player", as in `$<.prop>` or `$.prop`. So, for example:

```
$a walk[sing=s] off the edge of the world.
```

Will conjugate `walk` or `walks` based on the tense of `a`'s pronouns (more
accurately, the pronouns of the Player bound to `a`), and

```
$a shoot[sing=s] <p> $.rangewep at $<b>.
```

Will use `a`'s property `rangewep`, as well as `a`'s possessive pronoun.

# Running

The program understands three arguments:

- `cat`: Read the world, and write it out. This is mostly useful for validation
  and testing. Provided the world is valid, the output will be _semantically_
  equivalent to the input, but might not be syntactically equivalent; in
  particular, whitespace may be rearranged, attributes and properties may be
  reordered, and some `message` references may be rewritten.
- `try_events`: Try to run every Event defined in the World. This still
  requires you define enough Players for every Event, but it will ignore the
  matchers in `needs`. It's useful to make sure your events correctly format
  themselves.
- `try_event`: Given an `Event` identifier, followed by as many
  `needid:playerid` as needed, this binds and prints a specific event after the
  result of the changes to the world. It's useful for testing a very specific
  circumstance. `needid` matches an identifier in `needs`, and `playerid`
  matches the identifier (not the name!) of a `Player` entry.
- `round`: Actually run a round, as described in the main loop above. The state
  of the world after the round is printed, followed by `---` (which prevents
  further parsing), followed by the messages emitted. This form is sufficient
  to pass to `round` again to make further progress.

# Building

Use `make`. This should work in any modestly modern UNIX system with a C++
compiler that knows the C++20 standard. I recommend WSL on Windows for
simplicity, if you don't already have a Cygwin prefix.

# Theory

This section briefly discusses the probability theory involved with the
simulation. It is not at all necessary to know this to use the tool, or even to
make worlds, although some of the information here may prove useful for those
developing sets of events.

When a simulation round is done, all "player" events (those that have at least
one needed actor) and all players are "shuffled" using an implementation of the
Mersenne Twister known as MT19937, a pseudorandom number generator (PRNG) with
very good uniform random properties in its sample set, here unsigned 32-bit
integers. This PRNG is seeded initially with state from the `std::random_dev`
generator, often your platforms cryptographically-secure PRNG (CSPRNG) provided
via `/dev/urandom` or `CryptGenRandom`. The shuffle itself is in the standard
C++ library, but is likely a [Fisher-Yates][fy] shuffle, which has the property
of making the probability of every permutation (ordering of the elements)
uniform up to the uniformity of the underlying RNG. In practice, most FY
shuffles implement "uniform" sampling within a fixed range using the modulus
(`%`) operator, and this often introduces a very slight bias toward the lower
numbers if the range's size isn't divisible by the state size. In a typical
"in-place" implementation of FY, this has a very, very slight tendency to
reverse the array, putting the last elements toward the front, although this is
a very complex topic and the biases so introduced (or not, depending on your
platform and the rigor of your libstd++ implementation) will likely have no
tangible effect on the simulation for almost all intents and purposes.

After the shuffles are done, player events are picked in order until no players
or player events remain. Thus, with enough possible events, every player should
participate in one event every round. Binding is done eagerly; in the order
specified, each "needs" slot is bound to the first matching player in the
players list, and that element is removed (preventing them from being rebound
elsewhere). Since the matching predicates in the "needs" section are
independent of the order, every permutation being equally likely implies every
arbitrarily-partitioned subset permutation is equally likely--so events that
have appropriately flexible choices of actors will tend to pick every possible
set of players they can over time.

As players are removed when bound to events, this means that later events may
fail to bind because their "needs" predicates no longer match any valid target.
This is especially likely to happen when there are few matching players to
begin with. This is intentional; it is expected that events with "rare matches"
be concommitantly "rare" to occur, and the simulator makes no attempt to
maximize the number of events it can retire. In pathological cases, this can
lead to the event list being exhausted before the player list, such that not
every player is mentioned or used in an event. For this reason, the size of the
event list should be designed to be significantly larger than the player list.
Each event is a unique object, however they can have the same details (needs,
world, and message sections), which allows the simple use of multiplicity
should some events be more "likely" than others, especially if they may be used
as fallbacks.

# Copyright

Grissess, 2025. Licensed under the GPL-3; see COPYING for details.

[sim]: https://brantsteele.net/hungergames/disclaimer.php
[fy]: https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
