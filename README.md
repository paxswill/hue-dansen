#Hue-dansen

###Alternatively, Caramellights.

This is a quick and dirty (there's pretty minimal error handling) tool to make
Philips Hue lights quickly flash different colors similar to the
[Caramelldansen Lights][kym] meme. 

[kym]: https://knowyourmeme.com/memes/caramelldansen-lights

When I had this idea, I thought, "Oh, this'll be easy to whip up a little
script in Python, right?" Ehh, no. My first attempt was to use the standard Hue
[REST API][hue-rest]. The probem with this approach is that it's slow, with
a decent amount of latency. Luckily there's the [Hue Entertainment][hue-ent]
API for low-latency, high frequency updates. The details of how it works is
pretty interesting, but as I read through the documentation I encountered DTLS.
I'd never heard of it before, and thought that it would be pretty easy to use,
maybe install a package from PyPI for it...not so much.

[hue-rest]: https://developers.meethue.com/develop/hue-api/
[hue-ent]: https://developers.meethue.com/develop/hue-entertainment/philips-hue-entertainment-api/

The Python standard library `ssl` module doesn't implement DTLS. There are a
pair of DTLS libraries on PyPI, but they havent been updated in a while (one
was Python 2 only, the other didn't seem like it supported DTLSv1.2).
Hue Entertainment also uses DTLS with pre-shared keys, not certificates like
most other implementations assume. The Python [`cryptography`][py-crypto] library
exposes some of functions and constants I needed, but not all of them.

[py-crypto]: https://cryptography.io/en/latest/hazmat/bindings/openssl/

From here I jumped into writing a tool in C using OpenSSL. Since OpenSSL is no
longer included with macOS (more accurately, the headers aren't, and Apple
discourges using the included library), I used the version installed by
Homebrew. From there I cobbled together just enough of a program to cycle the
lights through four colors at 165 BPM. Two very helpful pages were Christopher
Wood's [DTLS with OpenSSL][chris-wood] blog post, and especially
jxck's [OpenSSL DTLS API Gist][jxck]. Digging through the OpenSSL man pages
filled in the rest ([`SSL_CTX_set_psk_client_callback`][openssl-man] took some
especially close reading).

[chris-wood]: https://chris-wood.github.io/2016/05/06/OpenSSL-DTLS.html
[jxck]: https://gist.github.com/Jxck/b211a12423622fe304d2370b1f1d30d5
[openssl-man]: https://www.openssl.org/docs/man1.1.1/man3/SSL_CTX_set_psk_client_callback.html

## Compiling

Right now the Makefile assumes you're using macOS and have OpenSSL 1.1.x
installed through Homebrew, but it's pretty easy to change that (literally the
first line). From there a plain `make` should build it all for you.

The IDs of the lights to change is also currently hardcoded (remember where I
said this was quick and dirty?), so you will have to explore the Hue API and
find the appropriate IDs and change the source if you want to use this.

## Usage

This tool does precisely one thing, which is spam DTLS messages to a Hue bridge
to cycle through four colors at 2.75Hz (165 BPM). It does not register an
application with a Hue bridge, detect which lights to use, or enable the stream
mode through the Hue API. I did all those manually through the Hue CLIP
debugging tool (`http://<Hue bridge IP>/debug/clip.html`, and then started
`hue-dansen` once stream mode was active.

Once you've registered an application with *with a client key*, and stream mode
is active (it will disable itself after a few second of inactivity, so be
quick!) you can start `hue-dansen` like this:

    ./hue-dansen <hue IP address> <identity string> <PSK>

An optional duration in whole seconds can be added at the end as well. If you
don't add one, just interrupt the program to stop the blinkenlights.
