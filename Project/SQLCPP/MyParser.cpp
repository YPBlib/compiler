#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include<exception>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include<exception>
#include"llvmsql.h"
#include"catalog.h"
using namespace llvm;

/// SQL begin



/// SQL end

using LR = std::vector<std::unique_ptr<ExprAST>> ;
using LL = std::vector<std::unique_ptr<ExprAST>> ;
token currtoken;
void consumeit(std::vector<int> v, std::string s);

void consumeit(std::vector<int> v, std::string s)
{
	if (currtoken.token_kind == symbol)
	{
		int m = currtoken.token_value.symbol_mark; 
		if (std::find(v.cbegin(),v.cend(),m)!=v.cend())
		{
			currtoken = gettok();
			return;
		}
	}
	throw std::runtime_error(s);
}

void init_parser()
{
	currtoken = gettok();
}

bool iscompop(const int& i)
{
	return i == eq_mark || i == gteq_mark || i == gt_mark || i == lteq_mark || i == lt_mark || i == ltgt_mark || i == noteq_mark;
}

std::unique_ptr<ExprAST> ParseExprAST()
{
	auto lhs = ParseExpAST();
	std::unique_ptr<ExprAST> rhs;
	int op;
	if (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == and_mark ||
			currtoken.token_value.symbol_mark == andand_mark ||
			currtoken.token_value.symbol_mark == or_mark ||
			currtoken.token_value.symbol_mark == oror_mark)
		)
	{
		op = currtoken.token_value.symbol_mark;
		rhs = ParseExprAST();
		return llvm::make_unique<ExprAST>(std::move(lhs), op, std::move(rhs));
	}
	else
		return llvm::make_unique<ExprAST>(std::move(lhs),0,nullptr);
}



std::unique_ptr<ExpAST> ParseExpAST()
{
	std::unique_ptr<ExprAST> expr ;
	std::unique_ptr<BooleanPrimaryAST> bp ;
	if (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == tok_NOT || currtoken.token_value.symbol_mark == not_mark))
	{
		currtoken = gettok();	// consume ! or NOT
		expr = ParseExprAST();
	}
	else
	{
		bp = ParseBPAST();
	}
	return llvm::make_unique<ExpAST>(std::move(expr), std::move(bp));
}

std::unique_ptr<BooleanPrimaryAST> ParseBPAST()
{
	
	auto p = ParsePredicateAST();
	auto bp = llvm::make_unique<BooleanPrimaryAST>(nullptr, std::move(p), 0, 0, nullptr);
	while (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == tok_IS || iscompop(currtoken.token_value.symbol_mark))
		)
	{
		if (currtoken.token_value.symbol_mark == tok_IS)
		{
			currtoken = gettok();	// consume IS
			int flag = 0;
			if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == tok_NOT)
			{
				flag = tok_NOT;
				currtoken = gettok();	// consume NOT
				if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == tok_NULL)
				{
					currtoken = gettok();	// consume NULL
				}
				else
				{
					throw std::runtime_error(R"( parse error, do you mean "NOT NULL"?\n)");
				}
				bp=llvm::make_unique<BooleanPrimaryAST>(std::move(bp),nullptr, flag,0,nullptr);
			}
			else if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == tok_NULL)
			{
				flag = tok_NULL;
				currtoken = gettok();	// consume NULL
				bp = llvm::make_unique<BooleanPrimaryAST>(std::move(bp), nullptr, flag, 0, nullptr);
			}
			else
			{
				throw std::runtime_error("expect IS [NOT] NULL \n");
			}
		}
		else
		{
			int op = currtoken.token_value.symbol_mark;
			currtoken = gettok();	// consume op
			if ((currtoken.token_kind == symbol &&
				(currtoken.token_value.symbol_mark == tok_ALL || currtoken.token_value.symbol_mark == tok_ANY)
				))
			{
				int flag = currtoken.token_value.symbol_mark;
				currtoken = gettok();	// consume all/any
				if(currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == left_bracket_mark)
				{
					currtoken = gettok();	// consume '('
				}
				else
				{
					throw std::runtime_error("expext '( subquery)' pattern\n");
				}
				auto sub = ParseSubqueryAST();
				if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == right_bracket_mark)
				{
					currtoken = gettok();	// consume ')'
				}
				else
				{
					throw std::runtime_error("expext '( subquery)' pattern\n");
				}
				bp = llvm::make_unique<BooleanPrimaryAST>(std::move(bp), std::move(p), flag, op, std::move(sub));
			}
			else
			{
				p= ParsePredicateAST();
				bp = llvm::make_unique<BooleanPrimaryAST>(std::move(bp), std::move(p), 0, op, nullptr);
			}
		}
	}
	return llvm::make_unique<BooleanPrimaryAST>(std::move(bp), nullptr, 0, 0, nullptr);
}

std::unique_ptr<PredicateAST> ParsePredicateAST()
{
	auto bitexpr = ParseBitExprAST();
	bool flag = true;
	std::unique_ptr<BitExprAST> rhs;
	std::unique_ptr<SubqueryAST> sub;
	std::vector<std::unique_ptr<ExprAST>> exprs;
	if (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == tok_NOT ||
			currtoken.token_value.symbol_mark == tok_REGEXP || currtoken.token_value.symbol_mark == tok_IN))
	{
		if (currtoken.token_value.symbol_mark == tok_NOT)
		{
			currtoken = gettok();	//consum NOT
			flag = false;
		}
		else if (currtoken.token_value.symbol_mark == tok_REGEXP)
		{
				currtoken = gettok();	//consume REGEXP
				rhs = ParseBitExprAST();
		}
		else if (currtoken.token_value.symbol_mark == tok_IN)
		{
			currtoken = gettok();	//consume IN
			consumeit({ left_bracket_mark }, "expect '('\n");
			if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == tok_SELECT)
			{
				sub = ParseSubqueryAST();
			}
			else
			{
				auto expr = ParseExprAST();
				exprs.push_back(std::move(expr));
				while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
				{
					currtoken = gettok();	//consume ','
					expr = ParseExprAST();
					exprs.push_back(std::move(expr));
				}
				consumeit({ right_bracket_mark }, "expect ')'\n");
			}
		}
	}
	return llvm::make_unique<PredicateAST>(std::move(bitexpr), flag, std::move(rhs), std::move(sub), std::move(exprs));
}

std::unique_ptr<BitExprAST> ParseBitExprAST()
{
	auto bitexp = ParseBitExpAST();
	int op = 0;
	std::unique_ptr<BitExprAST> bitexpr;
	while (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == plus_mark || currtoken.token_value.symbol_mark == minus_mark))
	{
		op = currtoken.token_value.symbol_mark;
		bitexpr = llvm::make_unique<BitExprAST>(std::move(bitexpr), op, std::move(bitexp));
		currtoken = gettok();	// consume '-' or  '+ '
		bitexp = ParseBitExpAST();
	}
	return llvm::make_unique<BitExprAST>(std::move(bitexpr),op, std::move(bitexp));
}

std::unique_ptr<BitExpAST> ParseBitExpAST()
{
	auto bitex = ParseBitExAST();
	int op = 0;
	std::unique_ptr<BitExpAST> bitexp;
	while (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == mult_mark || currtoken.token_value.symbol_mark == div_mark
			|| currtoken.token_value.symbol_mark == mod_mark))
	{
		op = currtoken.token_value.symbol_mark;
		bitexp = llvm::make_unique<BitExpAST>(std::move(bitexp), op, std::move(bitex));
		currtoken = gettok();	// consume '*' or  '/ ' or '%'
		bitex = ParseBitExAST();
	}
	return llvm::make_unique<BitExpAST>(std::move(bitexp),op, std::move(bitex));
}

std::unique_ptr<BitExAST> ParseBitExAST()
{
	int mark = 0;
	std::unique_ptr<BitExAST> bitex;
	std::unique_ptr<SimpleExprAST> SE;
	if (!(currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == plus_mark || currtoken.token_value.symbol_mark == minus_mark)))
	{
		SE = ParseSEAST();
		return llvm::make_unique<BitExAST>(mark, std::move(bitex), std::move(SE));
	}
	while (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == plus_mark || currtoken.token_value.symbol_mark == minus_mark))
	{
		mark = currtoken.token_value.symbol_mark;
		currtoken = gettok();	//consume mark
		bitex = ParseBitExAST();
		bitex = llvm::make_unique<BitExAST>(mark, std::move(bitex), std::move(SE));
	}
	return llvm::make_unique<BitExAST>(mark, std::move(bitex), std::move(SE));
}

std::unique_ptr<SimpleExprAST> ParseSEAST()
{
	if (currtoken.token_kind == literal_double || currtoken.token_kind == literal_string
		|| currtoken.token_kind == literal_int)
	{
		auto lit = ParseLiteralAST();
		return llvm::make_unique<SimpleExprAST>(std::move(lit));
	}
	else if (currtoken.token_kind == id)
	{
		auto idstr = ParseIdAST();
		if (currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == dot_mark)
		{
			currtoken = gettok();	//s=consume '.'
			auto colname = ParseIdAST();
			auto tbcol=llvm::make_unique<TablecolAST>(std::move(idstr->id),std::move(colname->id));
			return llvm::make_unique<SimpleExprAST>(std::move(tbcol));
		}
		else if ( currtoken.token_kind == symbol &&currtoken.token_value.symbol_mark == left_bracket_mark)
		{
			currtoken = gettok();	//s=consume '('
			auto call = ParseCallAST(std::move(idstr));
			return llvm::make_unique<SimpleExprAST>(std::move(call));
		}
		else
		{
			return llvm::make_unique<SimpleExprAST>(std::move(idstr));
		}
	}
	else if (currtoken.token_kind == symbol)
	{
		if (currtoken.token_value.symbol_mark == tok_EXISTS)
		{
			auto exsub = ParseExistsSubqueryAST();
			return llvm::make_unique<SimpleExprAST>(std::move(exsub));
		}
		else if(currtoken.token_value.symbol_mark == left_bracket_mark)
		{
			currtoken = gettok();	// consume '('
			auto sub = ParseSubqueryAST();
			currtoken = gettok();	// consume '('
			return llvm::make_unique<SimpleExprAST>(std::move(sub));
		}
		else
		{
			throw std::runtime_error("expect (subquery) or EXISTS (subquery)\n");
		}
	}
	else
	{
		throw std::runtime_error("expect simple expression");
	}
}

std::unique_ptr<ParenExprAST> ParseParenExprAST()
{
	currtoken = gettok();	// consume '('
	auto e = ParseExprAST();
	consumeit({ right_bracket_mark }, "expect ')'\n");
	return llvm::make_unique<ParenExprAST>(std::move(e));
}

std::unique_ptr<LiteralAST> ParseLiteralAST()
{
	if (currtoken.token_kind == literal_double)
		return llvm::make_unique<LiteralAST>(ParseDoubleLiteralAST());
	if (currtoken.token_kind == literal_int)
		return llvm::make_unique<LiteralAST>(ParseIntLiteralAST());
	if (currtoken.token_kind == literal_string)
		return llvm::make_unique<LiteralAST>(ParseStringLiteralAST());
}

std::unique_ptr<StringLiteralAST> ParseStringLiteralAST()
{
	std::string temps = currtoken.token_value.string_literal;
	currtoken = gettok(); // consume 1 string token
						  // wait,  this string may be concat
	while (currtoken.token_kind == literal_string)
	{
		temps += currtoken.token_value.string_literal;
		currtoken = gettok(); // consume 1 string token
	}
	auto s = llvm::make_unique<std::string>(temps);
	auto result = llvm::make_unique<StringLiteralAST>(std::move(s));
	return result;
}

std::unique_ptr<IntLiteralAST> ParseIntLiteralAST()
{
	auto result = llvm::make_unique<IntLiteralAST>(llvm::make_unique<int>(currtoken.token_value.int_literal));
	currtoken = gettok(); // consume 1 int token
	return result;
}

std::unique_ptr<DoubleLiteralAST> ParseDoubleLiteralAST()
{
	auto result = llvm::make_unique<DoubleLiteralAST>(llvm::make_unique<double>(currtoken.token_value.double_literal));
	currtoken = gettok(); // consume 1 double token
	return result;
}

std::unique_ptr<IdAST> ParseIdAST()
{
	if (currtoken.token_kind != id)
	{
		throw std::runtime_error("expect identifier \n)");
	}
	auto result = llvm::make_unique<IdAST>(llvm::make_unique<std::string>(currtoken.token_value.IdentifierStr));
	currtoken = gettok(); // consume 1 id
	return result;
};

std::unique_ptr<CallAST> ParseCallAST(std::unique_ptr<IdAST> callee)
{
	consumeit({ left_bracket_mark }, "expect '(' when parsing arg list");
	std::vector<std::unique_ptr<ExprAST>> args;
	args.push_back(ParseExprAST());
	while (currtoken.token_kind == symbol && currtoken.token_value.symbol_mark != comma_mark)
	{
		currtoken = gettok();    // consume 1 comma token
		args.push_back(ParseExprAST());
	}
	consumeit({ left_bracket_mark }, "expect '(' when parsing arg list");
	return llvm::make_unique<CallAST>(std::move(callee), std::move(args));
}

std::unique_ptr<CallAST> ParseCallAST()
{
	return ParseCallAST(ParseIdAST());
}

std::unique_ptr<TablecolAST> ParseTablecolAST()
{
	std::unique_ptr<std::string> table_name;
	std::unique_ptr<std::string> col_name;
	auto temp_name = std::move(ParseIdAST()->id);
	if (currtoken.token_kind == blank || currtoken.token_kind == dot_mark)
	{
		currtoken = gettok();	// consume '.'
		table_name = std::move(temp_name);
		col_name = std::move(ParseIdAST()->id);
	}
	return llvm::make_unique<TablecolAST>(std::move(table_name), std::move(col_name));
}

std::unique_ptr< ExistsSubqueryAST> ParseExistsSubqueryAST()
{
	currtoken = gettok();  // consume 'EXISTS' reserved word
	currtoken = gettok();  // consume '(' reserved word
	auto subquery = ParseSubqueryAST();
	currtoken = gettok();  // consume ')' reserved word
	return llvm::make_unique<ExistsSubqueryAST>(std::move(subquery));
};

std::unique_ptr<SubqueryAST> ParseSubqueryAST()
{
	currtoken = gettok();    // consume 'SELECT' reserved word
	bool distinct_flag=false, from_flag=false, where_flag=false, having_flag=false, group_flag=false, order_flag=false;
	std::vector<std::unique_ptr<SelectExprAST>> exprs;
	std::unique_ptr<TableRefsAST> tbrefs;
	std::unique_ptr<ExprAST> wherecond;
	std::vector<std::unique_ptr<ExprAST>> groupby_exprs;
	std::unique_ptr<ExprAST> havingcond;
	std::vector<std::unique_ptr<ExprAST>> orderby_exprs;
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_DISTINCT)
	{
		currtoken = gettok();	// consume DISTINCT
	}
	if (!(currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == mult_mark))
	{
		exprs.push_back(ParseSelectExprAST());
		while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
		{
			currtoken = gettok();	// consume ','
			exprs.push_back(ParseSelectExprAST());
		}
	}
	else
	{
		currtoken = gettok();	// consume '*'
		consumeit({ tok_FROM }, "expect from");
	}
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_FROM)
	{
		from_flag = true;
		tbrefs = ParseTableRefsAST();

		if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_WHERE)
		{
			where_flag = true;
			currtoken = gettok();	// consume WHERE
			wherecond = ParseExprAST();
		}

		if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_GROUP)
		{
			group_flag = true;
			currtoken = gettok();	// consume GROUP
			consumeit({ tok_BY }, "`BY` must follow `GROUP`\n");
			groupby_exprs.push_back(ParseExprAST());
			while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
			{
				currtoken = gettok();	// consume comma
				groupby_exprs.push_back(ParseExprAST());
			}
			if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_HAVING)
			{
				having_flag = true;
				currtoken = gettok();	// consume HAVING
				havingcond = ParseExprAST();
			}
		}

		if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_ORDER)
		{
			order_flag = true;
			currtoken = gettok();	// consume ORDER
			consumeit({ tok_BY }, "`BY` must follow `ORDER`\n");
		}

		orderby_exprs.push_back(ParseExprAST());
		while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
		{
			currtoken = gettok();	// consume comma
			orderby_exprs.push_back(ParseExprAST());
		}
	}
	
	return llvm::make_unique<SubqueryAST>(distinct_flag, std::move(exprs), from_flag, std::move(tbrefs),
		where_flag, std::move(wherecond), group_flag, std::move(groupby_exprs),
		having_flag, std::move(havingcond), order_flag, std::move(orderby_exprs) );
}

std::unique_ptr<OnJoinCondAST> ParseOnJoinCondAST()
{
	currtoken = gettok();	// consume `ON`
	return llvm::make_unique<OnJoinCondAST>(ParseExprAST());
}

std::unique_ptr<UsingJoinCondAST> ParseUsingJoinCondAST()
{
	currtoken = gettok();	//	consume `USING`
	currtoken = gettok();	//	consume `(`
	std::vector<std::unique_ptr<TablecolAST>> cols;
	cols.push_back(ParseTablecolAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		currtoken = gettok();	// consume ','
		cols.push_back(ParseTablecolAST());
	}
	consumeit({ right_bracket_mark }, "expect `)`");
	return llvm::make_unique<UsingJoinCondAST>(std::move(cols));
}

std::unique_ptr<SelectExprAST> ParseSelectExprAST()
{
	std::unique_ptr<ExprAST> expr;
	std::unique_ptr<IdAST> alias;
	expr = ParseExprAST();
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_AS)
	{
		currtoken = gettok();	//consume AS
		alias = ParseIdAST();
	}
	return llvm::make_unique<SelectExprAST>(std::move(expr), std::move(alias));
}

std::unique_ptr<TableRefsAST> ParseTableRefsAST()
{
	std::vector<std::unique_ptr<TableRefAST>> refs;
	refs.push_back(ParseTableRefAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		currtoken = gettok();	// consume comma
		refs.push_back(ParseTableRefAST());
	}
	return llvm::make_unique<TableRefsAST>(std::move(refs));
}

std::unique_ptr<TableRefAST> ParseTableRefAST()
{
	std::unique_ptr<TableFactorAST> tbfactor;
	std::unique_ptr<TRIJAST> trij;
	std::unique_ptr<TRLROJAST> trlroj;
	std::unique_ptr<TRNLROJAST> trnlroj;
	std::unique_ptr<TableRefAST> ref;
	tbfactor = ParseTableFactorAST();
	while (currtoken.token_kind == symbol&&(currtoken.token_value.symbol_mark == tok_INNER|| currtoken.token_value.symbol_mark == tok_CROSS
		||currtoken.token_value.symbol_mark == tok_JOIN|| currtoken.token_value.symbol_mark == tok_INNER
		|| currtoken.token_value.symbol_mark == tok_LEFT|| currtoken.token_value.symbol_mark == tok_RIGHT
		|| currtoken.token_value.symbol_mark == tok_NATURAL))
	{
		ref = llvm::make_unique<TableRefAST>(std::move(tbfactor), std::move(trij), std::move(trlroj), std::move(trnlroj));
		if (currtoken.token_value.symbol_mark == tok_NATURAL)
		{
			trnlroj = ParseTRNLROJAST(std::move(ref));
		}
		if (currtoken.token_value.symbol_mark == tok_LEFT || currtoken.token_value.symbol_mark == tok_RIGHT)
		{
			trlroj = ParseTRLROJAST(std::move(ref));
		}
		else
		{
			trij = ParseTRIJAST(std::move(ref));
		}
	}
	return llvm::make_unique<TableRefAST>(std::move(tbfactor), std::move(trij), std::move(trlroj), std::move(trnlroj));
}

std::unique_ptr<TRIJAST> ParseTRIJAST(std::unique_ptr<TableRefAST> ref)
{
	std::unique_ptr<TableFactorAST> factor;
	std::unique_ptr<JoinCondAST> cond;
	if (currtoken.token_kind == symbol && (currtoken.token_value.symbol_mark == tok_INNER
		|| currtoken.token_value.symbol_mark == tok_CROSS || currtoken.token_value.symbol_mark == tok_JOIN))
	{
		if (currtoken.token_value.symbol_mark == tok_INNER || currtoken.token_value.symbol_mark == tok_CROSS)
		{
			currtoken = gettok();	// consume INNER/CROSS
		}
		consumeit({ tok_JOIN }, "expect `JOIN`\n");
		factor = ParseTableFactorAST();
		if (currtoken.token_kind == symbol && (currtoken.token_value.symbol_mark == tok_ON
			|| currtoken.token_value.symbol_mark == tok_USING))
		{
			cond = ParseJoinCondAST();
		}
	}
	else
	{
		throw std::runtime_error("expect `INNER/CROSS/JOIN`\n");
	}
	return llvm::make_unique<TRIJAST>(std::move(ref), std::move(factor), std::move(cond));
}

std::unique_ptr<TRIJAST> ParseTRIJAST()
{
	return ParseTRIJAST(ParseTableRefAST());
}

std::unique_ptr<TRLROJAST> ParseTRLROJAST(std::unique_ptr<TableRefAST> lhs)
{
	std::unique_ptr<TableRefAST> rhs;
	std::unique_ptr<JoinCondAST> cond;
	int lr = 0;
	if (currtoken.token_kind == symbol && (currtoken.token_value.symbol_mark == tok_LEFT ||
		currtoken.token_value.symbol_mark == tok_RIGHT))
	{
		currtoken = gettok();	// consume  LEFT/RIGHT
		lr = currtoken.token_value.symbol_mark;
	}
	else
	{
		throw std::runtime_error("expect LEFT/RIGHT");
	}
	if (currtoken.token_kind == symbol && currtoken.token_value.symbol_mark == tok_OUTER)
	{
		currtoken = gettok();	// consume OUTER
	}
	consumeit({ tok_JOIN }, "expect `JOIN` \n");
	rhs = ParseTableRefAST();
	cond = ParseJoinCondAST();
	return llvm::make_unique<TRLROJAST>(std::move(lhs), std::move(rhs), std::move(cond), lr);
}

std::unique_ptr<TRLROJAST> ParseTRLROJAST()
{
	return ParseTRLROJAST(ParseTableRefAST());
}

std::unique_ptr<TRNLROJAST> ParseTRNLROJAST(std::unique_ptr<TableRefAST> ref)
{
	std::unique_ptr<TableFactorAST> factor;
	int lr = 0;
	consumeit({ tok_NATURAL }, "expect NATURAL\n");
	if (currtoken.token_kind == symbol && (currtoken.token_value.symbol_mark == tok_LEFT ||
		currtoken.token_value.symbol_mark == tok_RIGHT))
	{
		currtoken = gettok();	// consume  LEFT/RIGHT
		lr = currtoken.token_value.symbol_mark;
	}
	if (currtoken.token_kind == symbol && currtoken.token_value.symbol_mark == tok_OUTER)
	{
		currtoken = gettok();	// consume OUTER
	}
	consumeit({ tok_JOIN }, "expect `JOIN` \n");
	factor = ParseTableFactorAST();
	return llvm::make_unique<TRNLROJAST>(std::move(ref), std::move(factor),lr);

}

std::unique_ptr<TRNLROJAST> ParseTRNLROJAST()
{
	return  ParseTRNLROJAST(ParseTableRefAST());
}

std::unique_ptr<TableFactorAST> ParseTableFactorAST()
{
	std::unique_ptr<TableNameAST> tbname;
	std::unique_ptr<TableQueryAST> tbsub;
	std::unique_ptr<TableRefsAST> tbrefs; 
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_SELECT)
	{
		tbsub = ParseTableQueryAST();
	}
	else if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == left_bracket_mark)
	{
		currtoken = gettok();	// consume '('
		tbrefs = ParseTableRefsAST();
		consumeit({ right_bracket_mark }, "expect '(' \n");
	}
	else
	{
		tbname = ParseTableNameAST();
	}
	return llvm::make_unique<TableFactorAST>(std::move(tbname), std::move(tbsub), std::move(tbrefs));
}

std::unique_ptr<JoinCondAST> ParseJoinCondAST()
{
	std::unique_ptr<OnJoinCondAST> oncond;
	std::unique_ptr<UsingJoinCondAST> uselist;
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_ON)
	{
		oncond = ParseOnJoinCondAST();
	}
	else if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_USING)
	{
		uselist = ParseUsingJoinCondAST();
	}
	else
	{
		throw std::runtime_error("expect ON/USING\n");
	}
	return llvm::make_unique<JoinCondAST>(std::move(oncond), std::move(uselist));
}

std::unique_ptr<TableNameAST> ParseTableNameAST()
{
	std::unique_ptr<IdAST> tbname;
	std::unique_ptr<IdAST> alias;
	tbname = ParseIdAST();
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_AS)
	{
		currtoken = gettok();	// consume AS
		alias = ParseIdAST();
	}
	return llvm::make_unique<TableNameAST>(std::move(tbname), std::move(alias));
}

std::unique_ptr<TableQueryAST> ParseTableQueryAST()
{
	std::unique_ptr<SubqueryAST> subq;
	std::unique_ptr<IdAST> alias;
	subq = ParseSubqueryAST();
	consumeit({ tok_AS }, "expect AS");
	alias = ParseIdAST();
	return llvm::make_unique<TableQueryAST>(std::move(subq), std::move(alias));
}

std::unique_ptr<DatatypeAST> ParseDatatypeAST()
{
	int dtype = 0;
	int n = 0;
	if (currtoken.token_kind == symbol && (currtoken.token_value.symbol_mark == tok_INT ||
		currtoken.token_value.symbol_mark == tok_FLOAT || currtoken.token_value.symbol_mark == tok_DOUBLE ||
		currtoken.token_value.symbol_mark == tok_CHAR || currtoken.token_value.symbol_mark == tok_VARCHAR))
	{
		dtype = currtoken.token_value.symbol_mark;
		currtoken = gettok();	// consume INT/FLOAT/DOUBLE/CHAR/VARCHAR
	}
	if (currtoken.token_kind == symbol && currtoken.token_value.symbol_mark == left_bracket_mark)
	{
		consumeit({ left_bracket_mark }, "expect '('");
		if (currtoken.token_kind == literal_int)
		{
			n = currtoken.token_value.int_literal;
		}
		consumeit({ right_bracket_mark }, "expect ')'");
	}
	return llvm::make_unique<DatatypeAST>(dtype, n);
}

std::unique_ptr<RefdefAST> ParseRefdefAST()
{
	consumeit({ tok_REFERENCES }, "expect REFERENCES \n");
	std::unique_ptr<IdAST> tbname;
	std::vector<std::unique_ptr<IdAST>> colname;
	tbname = ParseIdAST();
	consumeit({ left_bracket_mark }, "expect '('\n");
	colname.push_back(ParseIdAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		currtoken = gettok();	// consume ','
		colname.push_back(ParseIdAST());
	}
	consumeit({ right_bracket_mark }, "expect ')'\n");
	return llvm::make_unique<RefdefAST>(std::move(tbname), std::move(colname));
}

std::unique_ptr<PrimaryAST> ParsePrimaryAST()
{
	std::vector<std::unique_ptr<IdAST>> cols;
	consumeit({ tok_PRIMARY }, "expect keyword PRIMARY \n");
	consumeit({ tok_KEY }, "expect keyword KEY \n");
	consumeit({ left_bracket_mark }, "expect '(' \n");
	cols.push_back(ParseIdAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		consumeit({ comma_mark }, "expect ',' \n");
		cols.push_back(ParseIdAST());
	}
	return llvm::make_unique<PrimaryAST>(cols);
}

std::unique_ptr<UniqueAST> ParseUniqueAST()
{
	std::vector<std::unique_ptr<IdAST>> cols;
	consumeit({ tok_UNIQUE }, "expect keyword UNIQUE \n");
	consumeit({ left_bracket_mark }, "expect '(' \n");
	cols.push_back(ParseIdAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		consumeit({ comma_mark }, "expect ',' \n");
		cols.push_back(ParseIdAST());
	}
	return llvm::make_unique<UniqueAST>(cols);
}

std::unique_ptr<ForeignAST> ParseForeignAST()
{
	std::vector<std::unique_ptr<IdAST>> cols;
	std::unique_ptr<RefdefAST> refdef;
	consumeit({ tok_PRIMARY }, "expect keyword PRIMARY \n");
	consumeit({ tok_KEY }, "expect keyword KEY \n");
	consumeit({ left_bracket_mark }, "expect '(' \n");
	cols.push_back(ParseIdAST());
	while (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == comma_mark)
	{
		consumeit({ comma_mark }, "expect ',' \n");
		cols.push_back(ParseIdAST());
	}
	refdef = ParseRefdefAST();
	return llvm::make_unique<ForeignAST>(cols, refdef);
	
}

std::unique_ptr<CreatedefAST> ParseCreatedefAST()
{
	;
}



std::unique_ptr<ColdefAST> ParseColdefAST()
{
	auto colname = std::move(ParseIdAST()->id);
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_INT)
	{

		currtoken = gettok();    // consume `INT`
		return llvm::make_unique<ColdefAST>(colname, literal_int, true, false, false);
	}
	if (currtoken.token_kind == symbol &&
		(currtoken.token_value.symbol_mark == tok_FLOAT || currtoken.token_value.symbol_mark == tok_DOUBLE))
	{

		currtoken = gettok();    // consume `DOUBLE`
		return llvm::make_unique<ColdefAST>(colname, literal_double, true, false, false);
	}
	if (currtoken.token_kind == symbol&&currtoken.token_value.symbol_mark == tok_CHAR)
	{
		currtoken = gettok();   // consume `CHAR`
		currtoken = gettok();   // consume `(`
		if (currtoken.token_kind == literal_int)
		{
			int n = currtoken.token_value.int_literal;
			currtoken = gettok();    // consume int literal
			currtoken = gettok();    // consume `)`
			while (currtoken.token_kind == blank || currtoken.token_kind == comment)
			{
				currtoken = gettok();
			}
			return llvm::make_unique<ColdefAST>(colname, literal_string, true, false, false, n);
		}
		throw std::runtime_error("char size must be int \n");

	}
	return nullptr;
}






std::unique_ptr<CreateTableSimpleAST> ParseCreateTableSimpleAST()
{
	while (currtoken.token_kind == blank || currtoken.token_kind == comment)
	{
		currtoken = gettok();
	}
	// consume ` CREATE TABLE `
	currtoken = gettok();
	currtoken = gettok();
	auto table_name = std::move(ParseIdAST()->id);
	// consume `(`
	currtoken = gettok();
	std::vector<std::unique_ptr<ColdefAST>> cols;
	auto col = ParseColdefAST();
	col->col_name = std::move(table_name);
	cols.push_back(std::move(col));
	while (currtoken.token_kind == symbol&& currtoken.token_value.symbol_mark == comma_mark)
	{
		// consume `,`
		currtoken = gettok();
		auto col = ParseColdefAST();
		col-> = table_name;
		cols.push_back(std::move(col));
	}
	// consume `)`
	currtoken = gettok();
	while (currtoken.token_kind == blank || currtoken.token_kind == comment)
	{
		currtoken = gettok();
	}
	return llvm::make_unique<CreateTableSimpleAST>(table_name, std::move(cols));
}





