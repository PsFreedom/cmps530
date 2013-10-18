var N = 100000000;
var i;
var out = new Array();

for (i = 0; i < N; i++)
{
	out[i] = ((i+i+2)*2)/3;
}

print("N = " + N + ", out[N-1] = " + out[N-1]);
