#include "entityslayer/EntityNode.h"
#include "io/BinaryWriter.h"
#include "serialcore.h"
#include <cassert>


thread_local std::vector<std::string_view> propertyStack;

void reserializer::Exec(EntNode& property, BinaryWriter& writeTo) const
{
	propertyStack.push_back(property.getName());




	propertyStack.pop_back();
	
}

void reserial::rs_bool(EntNode& property, BinaryWriter& writeTo)
{
	assert(property.getFlags() & EntNode::NF_Braces == 0);

	//std::strto

	writeTo.WriteLE(static_cast<uint8_t>(1)); // Leaf node
	writeTo.WriteLE(1);
	bool val = property.getValue() == "true";
	writeTo.WriteLE(static_cast<uint8_t>(val));
}
