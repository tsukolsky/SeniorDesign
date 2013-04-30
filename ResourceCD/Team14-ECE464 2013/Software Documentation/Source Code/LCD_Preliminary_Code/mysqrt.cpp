//Brian Kane
//My Squareroot function

double my_sqrt(double lastguess,double num)
{
	double minimum = 0.0001;
	double difference;

	if (num <= 0)
	{ return num; }

	double nextguess = (lastguess + (num / lastguess)) / 2.0;

	if (nextguess > lastguess)
	{ difference = nextguess-lastguess; }
	else
	{ difference = lastguess-nextguess; }

	if (difference >= minimum)
	{ return my_sqrt(nextguess,num); }
	else
	{ return nextguess; }
}