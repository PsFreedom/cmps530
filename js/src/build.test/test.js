var N = 10000;
var i;
var out = new Array();


//var start = new Date().getTime();

for (i = 0; i < N; i++)
{
	out[i] = i-1;
	//out[i] = i+1;
}

//var end = new Date().getTime() - start;
//print(" " + end);

print("N = " + N + ", out[N-1] = " + out[N-1]);
