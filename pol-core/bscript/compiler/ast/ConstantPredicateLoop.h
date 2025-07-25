#pragma once

#include "bscript/compiler/ast/LoopStatement.h"

namespace Pol::Bscript::Compiler
{
class Block;

// loop which gets only generated by the compiler if the predicate is a constant value
class ConstantPredicateLoop : public LoopStatement
{
public:
  ConstantPredicateLoop( const SourceLocation& source_location, std::string label,
                         std::unique_ptr<Block> block, bool endless );

  void accept( NodeVisitor& visitor ) override;
  void describe_to( std::string& ) const override;

  Block& block();
  bool is_endless() const;

private:
  bool _endless;
};

}  // namespace Pol::Bscript::Compiler
