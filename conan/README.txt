Here we store Conan recipes invoked by ../conanfily.py + .github/ workflow(s) in the rare/transient cases
where a certain recipe is not (yet, usually) in conan-center.  (Conceivably perhaps we need one of our own
or to modify one in conan-center, but as of this writing it has not been needed.)  ../conanfily.py shall
explain such cases, and/or a README inside each recipe's will do so.

Generally, we'd prefer this directory to be empty.

To the extent a given recipe *is* in it, we prefer it to be entirely someone else's work without our modifications.
(The precipitating/first example was a Boost version recipe from an in-progress conan-center Pull Request not yet
merged and hence not officially in conan-center proper.)  Our license header shall be omitted, as it
is not our code, and any (typically small if any) changes on top of this shall be explained in a nearby README.
