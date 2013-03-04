/*******************************************************************************\
| myDate.h
| Author: Todd Sukolsky
| Initial Build: 1/29/2013
| Last Revised: 3/4/2013
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: Stores current date
|--------------------------------------------------------------------------------
| Revisions:1/29- Initial build.
|			1/30- Added get functions for month,day,year.
|			3/4- Added checkValidityDate() function for troubleshooting and error
|				 checking.
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdlib.h>

using namespace std;

class myDate{
	public:
		myDate();
		myDate(int month,int day, int year);
		BYTE getMonths();
		WORD getYears();
		BYTE getDays();
		void setDate(int month,int day,int year);
		const char * getDate();
		
	protected:
		void addDays(int days);
		BOOL checkValidityDate();
		
	private:
		volatile int month,day,year;	
		char dateString[17];
		void setMonth(int month);
		void setDay(int day);
		void setYear(int year);
		void addMonths(int months);
		void addYears(int years);
};

myDate::myDate(){
	month = 0;
	day = 0;
	year = 0;
}

myDate::myDate(int month,int day,int year){
	setDate(month,day,year);
}

BYTE myDate::getMonths(){
	return (BYTE)month;
}

WORD myDate::getYears(){
	return (WORD)year;
}

BYTE myDate::getDays(){
	return (BYTE)day;
}

void myDate::setMonth(int month){
	if (month/13 == 0){
		this->month = month;
	}
}

void myDate::setDay(int day){
	if ((month == 9 || month == 4 || month == 6) && day/31 == 0){
		this->day = day;
	} else if (month == 2 && day/29 == 0){
		this->day = day;
	} else{
		if (day/32 == 0){
			this->day = day;
		}		
	}
}

void myDate::setYear(int year){
	if (year >= 2000){
		this->year = year;
	} else {
		this->year = 1010;
	}	
}

void myDate::setDate(int month, int day, int year){
	setMonth(month);
	setDay(day);
	setYear(year);
}


void myDate::addYears(int years){	//allows negative years to come in, therefore subtracting years
	volatile int tempYears = year + years;
	if (tempYears > 2012){
		year = tempYears;
	}
}

void myDate::addMonths(int months){	//adds months depending on number of months currently on.
	volatile int tempMonths = month + months;
	if (tempMonths > 12 && tempMonths < 25){
		int yearsToAdd = 1;
		addYears(yearsToAdd);
	} else if (tempMonths >= 25){
		int yearsToAdd = tempMonths/12;
		addYears(yearsToAdd);
	} else {
		month = tempMonths;
	}	
}

void myDate::addDays(int days){
	volatile int tempDays = days + day;
	//Logic for incrementing days the right way. We are assuming no more than one month will be added
	if (month == 9 || month == 4 || month == 6){	//30 days in a month
		if (tempDays/31 == 0){	
			day = tempDays;
		} else {
			addMonths(1);	//just add 1 month
			day = tempDays%31;			
		}
	} else if (month == 2){
		if(day/29 == 0){		//Don't take into account LeapYear
			day = tempDays;
		} else {
			addMonths(1);
			day = tempDays%29;
		}		
	} else {
		if (day/32 == 0){
			day = tempDays;
		} else {
			addMonths(1);
			day = tempDays%32;
		}
	}
}

BOOL myDate::checkValidityDate(){
	BOOL checkYear=fFalse;
	if (month/13==0){
			if ((month == 9 || month == 4 || month == 6) && day/31 == 0){
				checkYear=fTrue;
			} else if (month == 2 && day/29 == 0){
				checkYear=fTrue;
			} else{
				if (day/32 == 0){
					checkYear=fTrue;
				}
			}
			
			//Check year
			if (checkYear && year>=2013){return fTrue;}
			else {return fFalse;}
	} else {return fFalse;}
}

const char * myDate::getDate(){
	char monthString[3],dayString[3],yearString[5];
	itoa(month,monthString,10);
	itoa(day,dayString,10);
	itoa(year,yearString,10);
	strcpy(dateString,monthString);
	strcat(dateString,",");
	strcat(dateString,dayString);
	strcat(dateString,",");
	strcat(dateString,yearString);
	dateString[15] = ' ';
	dateString[16] = '\0';
	
	return dateString;
}