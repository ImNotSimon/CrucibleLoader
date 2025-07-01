#pragma once

class EntNode;
class BinaryWriter;

struct reserializer {
	void (*callback)(EntNode& property, BinaryWriter& writeTo) = nullptr;
	int arrayLength = 0;

	void Exec(EntNode& property, BinaryWriter& writeTo) const;
};

namespace reserial {

	/* */

	/* Primitive Types */
	void rs_bool(EntNode& property, BinaryWriter& writeTo);
	void rs_char(EntNode& property, BinaryWriter& writeTo);
	void rs_unsigned_char(EntNode& property, BinaryWriter& writeTo);
	void rs_wchar_t(EntNode& property, BinaryWriter& writeTo);
	void rs_short(EntNode& property, BinaryWriter& writeTo);
	void rs_unsigned_short(EntNode& property, BinaryWriter& writeTo);
	void rs_int(EntNode& property, BinaryWriter& writeTo);
	void rs_unsigned_int(EntNode& property, BinaryWriter& writeTo);
	void rs_long(EntNode& property, BinaryWriter& writeTo);
	void rs_long_long(EntNode& property, BinaryWriter& writeTo);
	void rs_unsigned_long(EntNode& property, BinaryWriter& writeTo);
	void rs_unsigned_long_long(EntNode& property, BinaryWriter& writeTo);
	void rs_float(EntNode& property, BinaryWriter& writeTo);
	void rs_double(EntNode& property, BinaryWriter& writeTo);
}