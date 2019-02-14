#include "parser.h"
#include<iostream>
int main()
{
	init_cata(); 
	initbuff();
	init_scanner();
	init_parser();
	
	int qwexqy = 0;
	qwexqy++;
	while (qwexqy)
	{
		try
		{
			auto z = ParseStatementAST();
			qwexqy++;
			if (z == nullptr)
				break;
		}
		catch (std::runtime_error& s)
		{
			std::cout << s.what() << std::endl;
			skip_exp();
		}
	}
	int ty = qwexqy;
	return 0;
}