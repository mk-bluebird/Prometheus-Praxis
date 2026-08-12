---- MODULE KerInvariant ----
EXTENDS Naturals, TLC

CONSTANT Score, Delta, Actions

VARIABLES k, e, r, delta_v, decision

vars == <<k, e, r, delta_v, decision>>

ValidScore(x) == x \in Score
ValidDecision(x) == x \in Actions

CandidateValid(nk, ne, nr, ndv, nd) ==
    /\ ValidScore(nk)
    /\ ValidScore(ne)
    /\ ValidScore(nr)
    /\ ndv \in Delta
    /\ nr >= ndv
    /\ ValidDecision(nd)

Init ==
    /\ k \in Score
    /\ e \in Score
    /\ r \in Score
    /\ delta_v \in Delta
    /\ r >= delta_v
    /\ decision \in Actions

Apply(nk, ne, nr, ndv, nd) ==
    IF CandidateValid(nk, ne, nr, ndv, nd)
    THEN /\ k' = nk
         /\ e' = ne
         /\ r' = nr
         /\ delta_v' = ndv
         /\ decision' = nd
    ELSE UNCHANGED vars

Next ==
    \E nk \in Score, ne \in Score, nr \in Score,
       ndv \in Delta, nd \in Actions :
        Apply(nk, ne, nr, ndv, nd)

TypeInvariant ==
    /\ ValidScore(k)
    /\ ValidScore(e)
    /\ ValidScore(r)
    /\ delta_v \in Delta
    /\ ValidDecision(decision)

RiskInvariant == r >= delta_v

Spec == Init /\ [][Next]_vars

THEOREM Spec => []TypeInvariant
THEOREM Spec => []RiskInvariant
====
